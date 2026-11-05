#ifndef __CHASSIS_CAN_H__
#define __CHASSIS_CAN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "can.h"
#include "stm32f4xx_hal_can.h"

typedef struct
{
	uint32_t can_id;
	int16_t set_current;
	uint16_t rotor_angle;
	int16_t rotor_speed;
	int16_t torque_current;
	uint8_t temp;
} chassis_motor_info_t;

#define CHASSIS_CAN_MOTOR_NUM 4

extern chassis_motor_info_t g_chassis_motors[CHASSIS_CAN_MOTOR_NUM];
extern uint32_t g_chassis_can_tx_drop_count;
extern volatile uint32_t g_can_rx1_count;      /* FIFO1 收到的总帧数(诊断) */
extern volatile uint16_t g_can_rx1_hi_stdid;   /* FIFO1 里见到的最大标准帧 ID(诊断) */
extern volatile uint32_t g_can_rx1_ext_addr_count[8]; /* 扩展帧按地址(ExtId高字节)计数, 0-7 */
extern volatile uint8_t  g_can_rx1_ext_last_data0[8]; /* 各地址最近一帧扩展帧的数据首字节 */
extern volatile uint8_t  g_can_rx1_ext_last_dlc[8];   /* 各地址最近一帧扩展帧的 DLC */
extern volatile int16_t  g_motor6_speed_rpm;          /* 电机6实测转速(RPM, 带符号), OLED 第8行诊断 */

void chassis_can_filter_init(void);
void chassis_can_send_currents(int16_t current1, int16_t current2, int16_t current3, int16_t current4);
void chassis_can_parse_xv2_speed_feedback(uint32_t ext_id, const uint8_t *rx_data, uint8_t dlc);
void chassis_can_rx_fifo1_callback(CAN_HandleTypeDef *hcan);
void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan);

#ifdef __cplusplus
}
#endif

#endif
