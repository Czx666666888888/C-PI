#include "crsf.h"

#include "stm32f4xx_hal.h"

/* 解析状态机 */
typedef enum
{
    CRSF_ST_ADDR = 0,   /* 等待帧头 0xC8 */
    CRSF_ST_LEN,        /* 读取长度字段 */
    CRSF_ST_TYPE,       /* 读取类型字段 */
    CRSF_ST_PAYLOAD,    /* 累积负载字节 */
    CRSF_ST_CRC         /* 读取校验字节并校验 */
} crsf_state_t;

static crsf_state_t s_state = CRSF_ST_ADDR;
static uint8_t      s_len = 0U;                       /* 长度字段值 (type+payload+crc) */
static uint8_t      s_idx = 0U;                       /* 当前已累积的负载字节数 */
static uint8_t      s_type = 0U;
static uint8_t      s_payload[CRSF_MAX_PAYLOAD_LEN];  /* 负载缓冲 */
static uint16_t     s_channels[CRSF_NUM_CHANNELS];    /* 16 通道原始值 */
static volatile uint32_t s_last_frame_tick = 0U;      /* 最近有效帧时刻 */
static volatile uint32_t s_byte_count  = 0U;          /* 累计收到的字节数 */
static volatile uint32_t s_frame_count = 0U;          /* 累计解码成功的有效 RC 帧数 */

static uint8_t crc8_dvb_s2(uint8_t crc, uint8_t byte)
{
    uint8_t i;
    crc ^= byte;
    for (i = 0; i < 8; i++)
    {
        if (crc & 0x80)
        {
            crc = (uint8_t)((crc << 1) ^ 0xD5);
        }
        else
        {
            crc = (uint8_t)(crc << 1);
        }
    }
    return crc;
}

/* 16 通道 × 11bit 位流解包 (标准 CRSF 布局) */
static void crsf_unpack_channels(const uint8_t *p, uint16_t *ch)
{
    ch[0]  = ((uint16_t)p[0]  | ((uint16_t)p[1]  << 8))              & 0x07FF;
    ch[1]  = (((uint16_t)p[1] >> 3) | ((uint16_t)p[2] << 5))        & 0x07FF;
    ch[2]  = (((uint16_t)p[2] >> 6) | ((uint16_t)p[3] << 2) | ((uint16_t)p[4] << 10)) & 0x07FF;
    ch[3]  = (((uint16_t)p[4] >> 1) | ((uint16_t)p[5] << 7))        & 0x07FF;
    ch[4]  = (((uint16_t)p[5] >> 4) | ((uint16_t)p[6] << 4))        & 0x07FF;
    ch[5]  = (((uint16_t)p[6] >> 7) | ((uint16_t)p[7] << 1) | ((uint16_t)p[8] << 9)) & 0x07FF;
    ch[6]  = (((uint16_t)p[8] >> 2) | ((uint16_t)p[9] << 6))        & 0x07FF;
    ch[7]  = (((uint16_t)p[9] >> 5) | ((uint16_t)p[10] << 3))       & 0x07FF;
    ch[8]  = ((uint16_t)p[11] | ((uint16_t)p[12] << 8))             & 0x07FF;
    ch[9]  = (((uint16_t)p[12] >> 3) | ((uint16_t)p[13] << 5))      & 0x07FF;
    ch[10] = (((uint16_t)p[13] >> 6) | ((uint16_t)p[14] << 2) | ((uint16_t)p[15] << 10)) & 0x07FF;
    ch[11] = (((uint16_t)p[15] >> 1) | ((uint16_t)p[16] << 7))      & 0x07FF;
    ch[12] = (((uint16_t)p[16] >> 4) | ((uint16_t)p[17] << 4))      & 0x07FF;
    ch[13] = (((uint16_t)p[17] >> 7) | ((uint16_t)p[18] << 1) | ((uint16_t)p[19] << 9)) & 0x07FF;
    ch[14] = (((uint16_t)p[19] >> 2) | ((uint16_t)p[20] << 6))      & 0x07FF;
    ch[15] = (((uint16_t)p[20] >> 5) | ((uint16_t)p[21] << 3))      & 0x07FF;
}

void crsf_init(void)
{
    uint8_t i;
    s_state = CRSF_ST_ADDR;
    s_len   = 0U;
    s_idx   = 0U;
    s_type  = 0U;
    s_last_frame_tick = 0U;
    for (i = 0; i < CRSF_NUM_CHANNELS; i++)
    {
        s_channels[i] = CRSF_CH_CENTER;
    }
}

void crsf_parse_byte(uint8_t byte)
{
    s_byte_count++;
    switch (s_state)
    {
    case CRSF_ST_ADDR:
        if (byte == CRSF_ADDR)
        {
            s_state = CRSF_ST_LEN;
        }
        break;

    case CRSF_ST_LEN:
        /* len 至少含 type+crc, 且不超过负载上限 */
        if (byte >= 2U && byte <= CRSF_MAX_FRAME_LEN)
        {
            s_len   = byte;
            s_idx   = 0U;
            s_state = CRSF_ST_TYPE;
        }
        else
        {
            s_state = CRSF_ST_ADDR;   /* 长度非法, 重新同步 */
        }
        break;

    case CRSF_ST_TYPE:
        s_type = byte;
        s_idx  = 0U;
        /* 无负载的帧(仅 type+crc)直接进入校验 */
        s_state = (s_len == 2U) ? CRSF_ST_CRC : CRSF_ST_PAYLOAD;
        break;

    case CRSF_ST_PAYLOAD:
        s_payload[s_idx++] = byte;
        if (s_idx >= (uint8_t)(s_len - 2U))
        {
            s_state = CRSF_ST_CRC;
        }
        break;

    case CRSF_ST_CRC:
    {
        uint8_t crc = crc8_dvb_s2(0U, s_type);
        uint8_t i;
        for (i = 0; i < (uint8_t)(s_len - 2U); i++)
        {
            crc = crc8_dvb_s2(crc, s_payload[i]);
        }
        if (crc == byte)
        {
            if (s_type == CRSF_TYPE_RC_CHANNELS && (s_len - 2U) == CRSF_RC_PAYLOAD_LEN)
            {
                crsf_unpack_channels(s_payload, s_channels);
                s_last_frame_tick = HAL_GetTick();
                s_frame_count++;
            }
        }
        s_state = CRSF_ST_ADDR;
        break;
    }

    default:
        s_state = CRSF_ST_ADDR;
        break;
    }
}

uint16_t crsf_get_channel(uint8_t index)
{
    if (index >= CRSF_NUM_CHANNELS)
    {
        return CRSF_CH_CENTER;
    }
    return s_channels[index];
}

bool crsf_get_channels(uint16_t *channels)
{
    uint8_t i;
    if (channels == NULL)
    {
        return false;
    }
    for (i = 0; i < CRSF_NUM_CHANNELS; i++)
    {
        channels[i] = s_channels[i];
    }
    return (s_last_frame_tick != 0U);
}

uint32_t crsf_last_frame_tick(void)
{
    return s_last_frame_tick;
}

bool crsf_is_linked(uint32_t timeout_ms)
{
    uint32_t age;
    if (s_last_frame_tick == 0U)
    {
        return false;
    }
    age = HAL_GetTick() - s_last_frame_tick;
    return (age < timeout_ms);
}

uint32_t crsf_byte_count(void)
{
    return s_byte_count;
}

uint32_t crsf_frame_count(void)
{
    return s_frame_count;
}
