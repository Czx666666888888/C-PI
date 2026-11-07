#include "pi_link.h"
#include "usb_cpi.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

#define RX_SIZE 256U
#define TARGET_TIMEOUT_MS 500U
#define HEARTBEAT_TIMEOUT_MS 1500U
#define STATUS_INTERVAL_MS 500U

typedef struct {
    uint32_t tick;
    uint8_t byte;
} rx_byte_t;

/* 单生产者/单消费者缓冲，保存接收时刻，任务延迟不能让旧帧重新变新。 */
static volatile rx_byte_t rx[RX_SIZE];
static volatile uint16_t rx_head, rx_tail;
static volatile uint8_t rx_fault;
static uint8_t frame[17];
static uint8_t used;
static uint32_t frame_tick, heartbeat_tick, status_tick;
static uint8_t heartbeat_seen;
static uint16_t tx_seq;
volatile pi_target_t g_pi_target;
volatile uint8_t g_pi_online;
volatile uint32_t g_pi_frame_errors, g_pi_rx_errors, g_pi_tx_errors;

static uint16_t get16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static void put16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static uint16_t crc16(const uint8_t *data, uint8_t length)
{
    uint16_t crc = 0xFFFFU;
    uint8_t i, bit;
    for (i = 0U; i < length; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (bit = 0U; bit < 8U; ++bit) {
            crc = (uint16_t)((crc << 1) ^ ((crc & 0x8000U) ? 0x1021U : 0U));
        }
    }
    return crc;
}

void pi_link_rx_byte_isr(uint8_t byte)
{
    uint16_t next = (uint16_t)((rx_head + 1U) % RX_SIZE);
    if (next == rx_tail) {
        pi_link_rx_error_isr();
        return;
    }
    rx[rx_head].byte = byte;
    rx[rx_head].tick = HAL_GetTick();
    __DMB();
    rx_head = next;
}

void pi_link_rx_error_isr(void)
{
    rx_fault = 1U;
    ++g_pi_rx_errors;
}

static uint8_t pop_byte(rx_byte_t *item)
{
    uint8_t ok = 0U;
    taskENTER_CRITICAL();
    if (rx_fault) {
        /* 丢字节/串口错误后清掉残帧，不用错误前的旧目标继续抓取。 */
        rx_tail = rx_head;
        rx_fault = 0U;
        used = 0U;
        g_pi_target.valid = 0U;
    }
    if (rx_tail != rx_head) {
        item->byte = rx[rx_tail].byte;
        item->tick = rx[rx_tail].tick;
        rx_tail = (uint16_t)((rx_tail + 1U) % RX_SIZE);
        ok = 1U;
    }
    taskEXIT_CRITICAL();
    return ok;
}

void pi_link_get_target(pi_target_t *target)
{
    uint32_t now;
    uint8_t fault;
    taskENTER_CRITICAL();
    *target = g_pi_target;
    fault = rx_fault;
    taskEXIT_CRITICAL();
    now = HAL_GetTick();
    if (fault || target->valid != 1U || target->age_ms > TARGET_TIMEOUT_MS ||
        (uint32_t)(now - target->last_rx_tick) > TARGET_TIMEOUT_MS - target->age_ms) {
        target->valid = 0U;
    }
}

uint8_t pi_link_heartbeat_online(void)
{
    return heartbeat_seen && (uint32_t)(HAL_GetTick() - heartbeat_tick) < HEARTBEAT_TIMEOUT_MS;
}

static void send_frame(uint8_t type, const uint8_t *payload, uint8_t length)
{
    uint8_t data[14]; /* ACK 12字节，STATUS 14字节。仅本任务发送。 */
    data[0] = 0xA5U; data[1] = 0x5AU; data[2] = 1U; data[3] = type;
    put16(&data[4], tx_seq++);
    data[6] = length;
    memcpy(&data[7], payload, length);
    put16(&data[7U + length], crc16(data, (uint8_t)(7U + length)));
    if (USB_CPI_Send(data, (uint16_t)(9U + length)) != 0) {
        ++g_pi_tx_errors;
    }
}

static void accept_frame(void)
{
    uint16_t seq = get16(&frame[4]);
    uint8_t ack[3];
    if (frame[3] == 1U) {
        heartbeat_tick = frame_tick;
        heartbeat_seen = 1U;
    } else {
        pi_target_t target;
        /* valid只能0/1，有效目标必须使用约定颜色；畸形帧不更新目标或ACK。 */
        if (frame[8] > 1U || (frame[8] == 1U && (frame[7] < 1U || frame[7] > 6U))) {
            ++g_pi_frame_errors;
            return;
        }
        target.color = frame[7];
        target.valid = frame[8];
        target.dx = (int16_t)get16(&frame[9]);
        target.dy = (int16_t)get16(&frame[11]);
        target.age_ms = get16(&frame[13]);
        target.seq = seq;
        target.last_rx_tick = frame_tick;
        if (target.age_ms > TARGET_TIMEOUT_MS) target.valid = 0U;
        taskENTER_CRITICAL();
        g_pi_target = target;
        taskEXIT_CRITICAL();
        put16(ack, seq); ack[2] = 0U;
        send_frame(3U, ack, 3U);
    }
}

static void parse_byte(rx_byte_t item)
{
    uint8_t size;
    /* 半帧长时间中断后重同步，防止残帧吞掉新的帧头。 */
    if (used && (uint32_t)(item.tick - frame_tick) > 100U) used = 0U;
    frame_tick = item.tick;
    frame[used++] = item.byte;
    for (;;) {
        if (!used) return;
        if (frame[0] != 0xA5U) goto discard;
        if (used < 2U) return;
        if (frame[1] != 0x5AU) goto discard;
        if (used < 7U) return;
        /* 下位机仅接收HEARTBEAT/TARGET，其他类型不执行。 */
        if (frame[2] != 1U || !((frame[3] == 1U && frame[6] == 4U) ||
                                  (frame[3] == 2U && frame[6] == 8U))) {
            ++g_pi_frame_errors;
            goto discard;
        }
        size = (uint8_t)(9U + frame[6]);
        if (used < size) return;
        if (crc16(frame, (uint8_t)(size - 2U)) != get16(&frame[size - 2U])) {
            ++g_pi_frame_errors;
            goto discard;
        }
        accept_frame();
        used = (uint8_t)(used - size);
        memmove(frame, &frame[size], used);
        continue;
    discard:
        --used;
        memmove(frame, &frame[1], used);
    }
}

void pi_link_process(void)
{
    rx_byte_t item;
    pi_target_t target;
    uint8_t status[5];
    uint16_t flags;
    uint16_t budget = RX_SIZE; /* 每拍有界，噪声洪泛也不占满任务。 */
    uint32_t now;
    while (budget-- && pop_byte(&item)) parse_byte(item);
    pi_link_get_target(&target);
    if (!target.valid) {
        taskENTER_CRITICAL();
        g_pi_target.valid = 0U;
        taskEXIT_CRITICAL();
    }
    g_pi_online = pi_link_heartbeat_online();
    now = HAL_GetTick();
    if ((uint32_t)(now - status_tick) >= STATUS_INTERVAL_MS) {
        status_tick = now;
        flags = (uint16_t)((g_pi_online ? 1U : 0U) |
                          (target.valid ? 2U : 0U) | (g_auto_red_grab_done ? 4U : 0U));
        status[0] = (uint8_t)g_control_mode;
        put16(&status[1], flags);
        put16(&status[3], 0U);
        send_frame(4U, status, 5U);
    }
}
