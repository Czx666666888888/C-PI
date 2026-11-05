#ifndef __CRSF_H__
#define __CRSF_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ── CRSF 协议常量 ────────────────────────────────────────────────
 * ELRS 接收机将遥控器信号转为 CRSF 串行协议, 420000 baud。
 * 帧结构: [地址 0xC8][长度 len][类型 type][负载 payload][CRC8 校验]
 *   - len = type + payload + crc 的字节数
 *   - CRC8 多项式 0xD5, 初值 0, 覆盖 [type][payload]
 * RC_CHANNELS_PACKED (type=0x16): 16 通道 × 11bit = 22 字节负载, 小端位打包。
 * 通道量程 172(最小) ~ 992(中位) ~ 1811(最大)。
 * ─────────────────────────────────────────────────────────────── */
#define CRSF_ADDR             0xC8
#define CRSF_TYPE_RC_CHANNELS 0x16

#define CRSF_NUM_CHANNELS     16
#define CRSF_RC_PAYLOAD_LEN   22     /* RC 帧负载字节数 */
#define CRSF_MAX_PAYLOAD_LEN  60     /* CRSF 负载上限 */
#define CRSF_MAX_FRAME_LEN    (CRSF_MAX_PAYLOAD_LEN + 2) /* type + payload + crc */

#define CRSF_CH_MIN           172
#define CRSF_CH_MAX           1811
#define CRSF_CH_CENTER        992

/* ── 接口 ─────────────────────────────────────────────────────── */

/* 复位解析状态机(可选, 上电调用一次即可) */
void crsf_init(void);

/* 喂入一个字节(由 UART1 RX 中断回调调用), 内部按帧解析 */
void crsf_parse_byte(uint8_t byte);

/* 取指定通道(0~15)的原始值, 无有效帧时返回中位 992 */
uint16_t crsf_get_channel(uint8_t index);

/* 复制 16 个通道原始值到 channels[16], 返回是否曾收到过有效帧 */
bool crsf_get_channels(uint16_t *channels);

/* 最近一帧有效 RC 数据的时刻(uwTick), 用于掉线/失控保护 */
uint32_t crsf_last_frame_tick(void);

/* 最近一帧是否在 timeout_ms 内(即链路是否正常) */
bool crsf_is_linked(uint32_t timeout_ms);

/* ── 诊断统计 (用于排查接收问题) ── */
uint32_t crsf_byte_count(void);    /* 累计收到的字节数 */
uint32_t crsf_frame_count(void);   /* 累计解码成功的有效 RC 帧数 */

#ifdef __cplusplus
}
#endif

#endif /* __CRSF_H__ */
