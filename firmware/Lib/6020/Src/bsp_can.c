#include "bsp_can.h"
#include "stm32f4xx_hal_can.h"

#define GM6020_CAN_TX_TIMEOUT_MS (2U)

volatile moto_info_t motor_yaw_info;
volatile uint16_t can_cnt;
volatile uint16_t g_6020_tx_drop_count = 0;  /* set_GM6020_motor_voltage 超时丢弃计数(诊断: 0x1FF 命令是否因 mailbox 满被丢) */
 
void can_filter_init(void)//筛选器配置
{
    CAN_FilterTypeDef can_filter_st = {0};
  uint16_t can_208 = (uint16_t)(0x208 << 5);
  can_filter_st.FilterActivation = ENABLE;
  can_filter_st.FilterMode = CAN_FILTERMODE_IDLIST;
  can_filter_st.FilterScale = CAN_FILTERSCALE_16BIT;
  can_filter_st.FilterIdHigh = can_208;
  can_filter_st.FilterIdLow = can_208;
  can_filter_st.FilterMaskIdHigh = can_208;
  can_filter_st.FilterMaskIdLow = can_208;
    can_filter_st.FilterBank = 0;
    can_filter_st.FilterFIFOAssignment = CAN_RX_FIFO0;
    HAL_CAN_ConfigFilter(&hcan1, &can_filter_st);
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
  CAN_RxHeaderTypeDef rx_header;
  uint8_t             rx_data[8];

  if (hcan->Instance != CAN1) return;

  /* 排空 FIFO0，避免消息堆积 */
  while (HAL_CAN_GetRxFifoFillLevel(hcan, CAN_RX_FIFO0) > 0)
  {
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, rx_data) != HAL_OK)
      break;

    can_cnt++;
    if (rx_header.StdId == 0x208)
    {
      motor_yaw_info.rotor_angle    = (uint16_t)((rx_data[0] << 8) | rx_data[1]);
      motor_yaw_info.rotor_speed    = (int16_t)((rx_data[2] << 8) | rx_data[3]);
      motor_yaw_info.torque_current = (int16_t)((rx_data[4] << 8) | rx_data[5]);
      motor_yaw_info.temp           = rx_data[6];
    }
  }
}

void set_GM6020_motor_voltage(CAN_HandleTypeDef* hcan,int16_t v1)
{
  CAN_TxHeaderTypeDef tx_header;
  uint8_t             tx_data[8] = {0};
  uint32_t            tx_mailbox = 0;
  uint32_t            start_tick = HAL_GetTick();
    
  tx_header.StdId = 0x1FF;
  tx_header.IDE   = CAN_ID_STD;
  tx_header.RTR   = CAN_RTR_DATA;
  tx_header.DLC   = 8;//数据长度为8字节
 
  tx_data[6] = (v1>>8)&0xFF;//高八位
  tx_data[7] =    (v1)&0xFF;//低八位

  while (HAL_CAN_GetTxMailboxesFreeLevel(hcan) == 0U)
  {
    if ((HAL_GetTick() - start_tick) >= GM6020_CAN_TX_TIMEOUT_MS)
    {
      g_6020_tx_drop_count++;
      return;
    }
  }

  while (HAL_CAN_AddTxMessage(hcan, &tx_header, tx_data, &tx_mailbox) != HAL_OK)
  {
    if ((HAL_GetTick() - start_tick) >= GM6020_CAN_TX_TIMEOUT_MS)
    {
      g_6020_tx_drop_count++;
      return;
    }
  }
}
