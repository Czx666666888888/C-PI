#ifndef PI_LINK_H
#define PI_LINK_H

#include <stdint.h>

typedef enum {
    CONTROL_MODE_AUTO = 0,
    CONTROL_MODE_MANUAL
} control_mode_t;

typedef struct {
    uint8_t color;
    uint8_t valid;
    int16_t dx;
    int16_t dy;
    uint16_t age_ms;
    uint16_t seq;
    uint32_t last_rx_tick;
} pi_target_t;

extern volatile control_mode_t g_control_mode;
extern volatile uint8_t g_auto_red_grab_done;
extern volatile pi_target_t g_pi_target;
extern volatile uint8_t g_pi_online;
/* 调试器可直接观察：错误帧、UART/缓冲错误和反馈发送失败次数。 */
extern volatile uint32_t g_pi_frame_errors;
extern volatile uint32_t g_pi_rx_errors;
extern volatile uint32_t g_pi_tx_errors;

void pi_link_rx_byte_isr(uint8_t byte);
void pi_link_rx_error_isr(void);
void pi_link_process(void); /* 仅 MAIN_Task 调用，反馈发送不在中断执行 */
void pi_link_get_target(pi_target_t *target);
uint8_t pi_link_heartbeat_online(void);
#endif
