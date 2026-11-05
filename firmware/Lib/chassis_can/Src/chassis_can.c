#include "chassis_can.h"

#include "Lift.h"
#include "bsp_can.h"

#define CHASSIS_CAN_TX_TIMEOUT_MS (5U)

chassis_motor_info_t g_chassis_motors[CHASSIS_CAN_MOTOR_NUM] = {0};

uint32_t g_chassis_can_tx_drop_count = 0U;

volatile uint32_t g_can_rx1_count = 0U;    /* FIFO1 收到的总帧数(诊断) */
volatile uint16_t g_can_rx1_hi_stdid = 0U; /* FIFO1 里见到的最大标准帧 ID(诊断) */
volatile uint32_t g_can_rx1_ext_addr_count[8] = {0U}; /* 扩展帧按地址(ExtId高字节)计数, 0-7(诊断后轮是否回包) */
volatile uint8_t  g_can_rx1_ext_last_data0[8] = {0U}; /* 各地址最近一帧扩展帧的数据首字节(诊断回包命令字) */
volatile uint8_t  g_can_rx1_ext_last_dlc[8] = {0U};   /* 各地址最近一帧扩展帧的 DLC */
volatile int16_t  g_motor6_speed_rpm = 0;             /* 电机6实测转速(RPM, 带符号: CCW=正/CW=负), OLED 第8行诊断 */

static bool chassis_can_wait_tx_mailbox(uint32_t timeout_ms)
{
	uint32_t start_tick = HAL_GetTick();

	while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0U)
	{
		if ((HAL_GetTick() - start_tick) >= timeout_ms)
		{
			return false;
		}
	}

	return true;
}

static bool chassis_can_add_tx_message_with_timeout(CAN_TxHeaderTypeDef *tx_header, uint8_t *tx_data, uint32_t *tx_mailbox, uint32_t timeout_ms)
{
	uint32_t start_tick = HAL_GetTick();

	while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0U)
	{
		if ((HAL_GetTick() - start_tick) >= timeout_ms)
		{
			return false;
		}
	}

	while (HAL_CAN_AddTxMessage(&hcan1, tx_header, tx_data, tx_mailbox) != HAL_OK)
	{
		if ((HAL_GetTick() - start_tick) >= timeout_ms)
		{
			g_chassis_can_tx_drop_count++;
			return false;
		}
	}

	return true;
}

static void chassis_can_config_filter_16bit_list(uint8_t bank, uint16_t id1, uint16_t id2, uint16_t id3, uint16_t id4, uint32_t fifo)
{
	CAN_FilterTypeDef filter = {0};

	filter.FilterBank = bank;
	filter.FilterMode = CAN_FILTERMODE_IDLIST;
	filter.FilterScale = CAN_FILTERSCALE_16BIT;
	filter.FilterIdHigh = (uint16_t)(id1 << 5);
	filter.FilterIdLow = (uint16_t)(id2 << 5);
	filter.FilterMaskIdHigh = (uint16_t)(id3 << 5);
	filter.FilterMaskIdLow = (uint16_t)(id4 << 5);
	filter.FilterFIFOAssignment = fifo;
	filter.FilterActivation = ENABLE;
	filter.SlaveStartFilterBank = 14;

	while (HAL_CAN_ConfigFilter(&hcan1, &filter) != HAL_OK)
	{
	}
}

static void chassis_can_config_filter_32bit_all_pass(uint8_t bank, uint32_t fifo)
{
	CAN_FilterTypeDef filter = {0};

	filter.FilterBank = bank;
	filter.FilterMode = CAN_FILTERMODE_IDMASK;
	filter.FilterScale = CAN_FILTERSCALE_32BIT;
	filter.FilterIdHigh = 0x0000;
	filter.FilterIdLow = 0x0000;
	filter.FilterMaskIdHigh = 0x0000;
	filter.FilterMaskIdLow = 0x0000;
	filter.FilterFIFOAssignment = fifo;
	filter.FilterActivation = ENABLE;
	filter.SlaveStartFilterBank = 14;

	while (HAL_CAN_ConfigFilter(&hcan1, &filter) != HAL_OK)
	{
	}
}

void chassis_can_filter_init(void)
{
	/* FIFO1 用于底盘电机回包，放行标准帧和扩展帧，具体在回调里按 IDE/ID 分发解析。 */
	chassis_can_config_filter_32bit_all_pass(1, CAN_RX_FIFO1);
	/* 中断激活移至 main.c，统一在 HAL_CAN_Start 之后调用。 */
	/* HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO1_MSG_PENDING); */
}

void chassis_can_send_currents(int16_t current1, int16_t current2, int16_t current3, int16_t current4)
{
	CAN_TxHeaderTypeDef tx_header = {0};
	uint8_t tx_data[8] = {0};
	uint32_t tx_mailbox = 0;

	tx_header.StdId = 0x200;
	tx_header.IDE = CAN_ID_STD;
	tx_header.RTR = CAN_RTR_DATA;
	tx_header.DLC = 8;

	tx_data[0] = (uint8_t)(current1 >> 8);
	tx_data[1] = (uint8_t)(current1);
	tx_data[2] = (uint8_t)(current2 >> 8);
	tx_data[3] = (uint8_t)(current2);
	tx_data[4] = (uint8_t)(current3 >> 8);
	tx_data[5] = (uint8_t)(current3);
	tx_data[6] = (uint8_t)(current4 >> 8);
	tx_data[7] = (uint8_t)(current4);

	if (!chassis_can_add_tx_message_with_timeout(&tx_header, tx_data, &tx_mailbox, CHASSIS_CAN_TX_TIMEOUT_MS))
	{
		return;
	}
}//保留的标准帧电机发送函数，当前底盘电机使用扩展帧发送，回包也在扩展帧里解析。

void can_SendCmd(__IO uint8_t *cmd, uint8_t len)
{
	static uint32_t tx_mailbox;
	uint8_t i = 0;
	uint8_t j = 0;
	uint8_t k = 0;
	uint8_t l = 0;
	uint8_t pack_num = 0;
	CAN_TxHeaderTypeDef tx_header = {0};
	uint8_t tx_data[8] = {0};

	j = (uint8_t)(len - 2U);

	while (i < j)
	{
		k = (uint8_t)(j - i);

		tx_header.StdId = 0x000;
		tx_header.ExtId = ((uint32_t)cmd[0] << 8) | (uint32_t)pack_num;
		tx_header.IDE = CAN_ID_EXT;
		tx_header.RTR = CAN_RTR_DATA;
		tx_data[0] = cmd[1];

		if (k < 8U)
		{
			for (l = 0; l < k; l++, i++)
			{
				tx_data[l + 1U] = cmd[i + 2U];
			}
			tx_header.DLC = (uint32_t)(k + 1U);
		}
		else
		{
			for (l = 0; l < 7U; l++, i++)
			{
				tx_data[l + 1U] = cmd[i + 2U];
			}
			tx_header.DLC = 8U;
		}

		if (!chassis_can_add_tx_message_with_timeout(&tx_header, tx_data, &tx_mailbox, CHASSIS_CAN_TX_TIMEOUT_MS))
		{
			g_chassis_can_tx_drop_count++;
			return;
		}

		if (!chassis_can_wait_tx_mailbox(CHASSIS_CAN_TX_TIMEOUT_MS))
		{
			g_chassis_can_tx_drop_count++;
			return;
		}

		pack_num++;
	}
}

void chassis_can_parse_xv2_speed_feedback(uint32_t ext_id, const uint8_t *rx_data, uint8_t dlc)
{
	uint8_t motor_addr;
	uint8_t index;
	uint8_t dir;
	uint16_t mag;

	/* S_VEL(0x35) 回包字节序: data[0]=0x35(命令字), data[1]=方向(0=CW,1=CCW,与指令dir一致),
	   data[2..3]=转速幅值(大端, 无符号, 单位 0.1RPM), data[4]=其它(暂不用)。
	   两个易错点:
	   1) 单位是 0.1RPM(与速度指令 X_V2_Vel_Control 里 v=vel*10 一致), 必须 /10 才是 RPM。
	      若当成整数 RPM 直接使用(不 /10), 反馈被放大 10 倍: 闭环看到"轮子飞快"→拼命减速→
	      轮子骤停→反馈归零→又拼命加速, 形成启停振荡 → 车一卡一卡、指令(T)无序乱跳。
	   2) 幅值是"无符号+独立方向字节", 不是 data[1..2] 拼成的有符号16位: 按有符号拼会把方向
	      字节当幅值高字节, CW(方向字节=0) 直接读成 0 → 该方向反馈恒为0、闭环误判。 */
	if (dlc < 4U)
	{
		return;
	}

	/* 按当前发送封装，扩展ID高位字节为电机地址。 */
	motor_addr = (uint8_t)((ext_id >> 8) & 0xFFU);
	if (motor_addr < 1U || motor_addr > 4U)
	{
		return;
	}

	/* 0x35 为读取实时转速参数。 */
	if (rx_data[0] != 0x35U)
	{
		return;
	}

	dir = rx_data[1];
	mag = (uint16_t)(((uint16_t)rx_data[2] << 8) | (uint16_t)rx_data[3]);
	index = (uint8_t)(motor_addr - 1U);

	g_chassis_motors[index].can_id = motor_addr;
	/* 方向字节 1(CCW) 记为正, 0(CW) 记为负; 幅值单位 0.1RPM, 除以 10 得 RPM。 */
	g_chassis_motors[index].rotor_speed = (dir != 0U) ? (int16_t)(mag / 10) : -(int16_t)(mag / 10);
}

void chassis_can_rx_fifo1_callback(CAN_HandleTypeDef *hcan)
{
	CAN_RxHeaderTypeDef rx_header;
	uint8_t rx_data[8];

	if (hcan->Instance != CAN1)
	{
		return;
	}

	while (HAL_CAN_GetRxFifoFillLevel(hcan, CAN_RX_FIFO1) > 0)
	{
		if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO1, &rx_header, rx_data) != HAL_OK)
		{
			return;
		}

		g_can_rx1_count++;

		if (rx_header.IDE == CAN_ID_STD)
		{
			if (rx_header.StdId > g_can_rx1_hi_stdid)
			{
				g_can_rx1_hi_stdid = rx_header.StdId;
			}

			if (rx_header.StdId == 0x208U)
			{
				/* 6020 反馈回退: 若滤波器意外把 0x208 送到了 FIFO1, 这里同样解析, 保证锁位有反馈 */
				motor_yaw_info.rotor_angle    = (uint16_t)((rx_data[0] << 8) | rx_data[1]);
				motor_yaw_info.rotor_speed    = (int16_t)((rx_data[2] << 8) | rx_data[3]);
				motor_yaw_info.torque_current = (int16_t)((rx_data[4] << 8) | rx_data[5]);
				motor_yaw_info.temp           = rx_data[6];
			}
			else if (rx_header.StdId >= 0x201U && rx_header.StdId <= 0x204U)
			{
				uint8_t index = (uint8_t)(rx_header.StdId - 0x201U);
				g_chassis_motors[index].can_id = rx_header.StdId;
				g_chassis_motors[index].rotor_angle = (uint16_t)((rx_data[0] << 8) | rx_data[1]);
				g_chassis_motors[index].rotor_speed = (int16_t)((rx_data[2] << 8) | rx_data[3]);
				g_chassis_motors[index].torque_current = (int16_t)((rx_data[4] << 8) | rx_data[5]);
				g_chassis_motors[index].temp = rx_data[6];
			}
		}
		else if (rx_header.IDE == CAN_ID_EXT)
		{
			uint8_t ext_addr = (uint8_t)((rx_header.ExtId >> 8) & 0xFFU);
			if (ext_addr < 8U)
			{
				g_can_rx1_ext_addr_count[ext_addr]++;
				g_can_rx1_ext_last_data0[ext_addr] = rx_data[0];
				g_can_rx1_ext_last_dlc[ext_addr] = (uint8_t)rx_header.DLC;
			}
			/* 电机6: 解析 S_VEL(0x35) 回包, 供 OLED 第8行显示实测转速。
			   S_VEL 回包字节序与四轮/Lift 一致: data[0]=0x35, data[1]=方向(0=CW,1=CCW),
			   data[2..3]=转速幅值(大端, 单位 0.1RPM → 必须 /10)。方向 1(CCW)记正, 0(CW)记负。 */
			if (ext_addr == 6U && rx_header.DLC >= 4U && rx_data[0] == 0x35U)
			{
				uint8_t  dir6  = rx_data[1];
				uint16_t mag6  = (uint16_t)(((uint16_t)rx_data[2] << 8) | (uint16_t)rx_data[3]);
				g_motor6_speed_rpm = (dir6 != 0U) ? (int16_t)(mag6 / 10) : -(int16_t)(mag6 / 10);
			}
			chassis_can_parse_xv2_speed_feedback(rx_header.ExtId, rx_data, (uint8_t)rx_header.DLC);
			lift_can_parse_xv2_feedback(rx_header.ExtId, rx_data, (uint8_t)rx_header.DLC);
		}
	}
}

void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
	chassis_can_rx_fifo1_callback(hcan);
}