#include "Lift.h"
#include "main.h"

#include "cmsis_os.h"

#include <math.h>

#include "X_V2.h"

#define LIFT_MOTOR_ID            (5U)
#define LIFT_FEEDBACK_REQ_DIV    (2U)
#define LIFT_DEFAULT_PITCH_MM    (2.0f)
#define LIFT_DEFAULT_MIN_MM      (-350.0f)
#define LIFT_DEFAULT_MAX_MM      (350.0f)
#define LIFT_DEFAULT_MAX_RPM     (300.0f)
#define LIFT_DEFAULT_ACC_RPMPS   (800.0f)
#define LIFT_DEFAULT_DEC_RPMPS   (800.0f)
#define LIFT_DEFAULT_DIR_SIGN    (-1)

typedef struct
{
	float height_mm;
	float speed_rpm;
	float target_height_mm;
	float zero_bias_mm;
	float motor_pos_rev;
	float screw_pitch_mm;
	float min_height_mm;
	float max_height_mm;
	float max_vel_rpm;
	float acc_rpmps;
	float dec_rpmps;
	int8_t cmd_dir_sign;
	uint8_t motor_enabled;
	uint8_t has_feedback;
	uint8_t feedback_req_div;
	uint8_t target_dirty;
	uint8_t motion_active;
	uint8_t test_state;
	float test_down_mm;
	float test_up_mm;
} lift_state_t;

typedef enum
{
	LIFT_TEST_IDLE = 0,
	LIFT_TEST_MOVING_DOWN,
	LIFT_TEST_MOVING_UP,
	LIFT_TEST_DONE
} lift_test_state_t;

static lift_state_t g_lift =
{
	.height_mm = 0.0f,
	.speed_rpm = 0.0f,
	.target_height_mm = 0.0f,
	.zero_bias_mm = 0.0f,
	.motor_pos_rev = 0.0f,
	.screw_pitch_mm = LIFT_DEFAULT_PITCH_MM,
	.min_height_mm = LIFT_DEFAULT_MIN_MM,
	.max_height_mm = LIFT_DEFAULT_MAX_MM,
	.max_vel_rpm = LIFT_DEFAULT_MAX_RPM,
	.acc_rpmps = LIFT_DEFAULT_ACC_RPMPS,
	.dec_rpmps = LIFT_DEFAULT_DEC_RPMPS,
	.cmd_dir_sign = LIFT_DEFAULT_DIR_SIGN,
	.motor_enabled = 0,
	.has_feedback = 0,
	.feedback_req_div = 0,
	.target_dirty = 0,
	.motion_active = 0,
	.test_state = LIFT_TEST_IDLE,
	.test_down_mm = 0.0f,
	.test_up_mm = 0.0f
};

static float lift_absf(float x)
{
	return (x >= 0.0f) ? x : -x;
}

static float lift_clampf(float x, float min_v, float max_v)
{
	if (x < min_v)
	{
		return min_v;
	}
	if (x > max_v)
	{
		return max_v;
	}
	return x;
}

static void lift_refresh_height_from_feedback(void)
{
	g_lift.height_mm = g_lift.zero_bias_mm + g_lift.motor_pos_rev * g_lift.screw_pitch_mm;
	g_lift.height_mm = lift_clampf(g_lift.height_mm, g_lift.min_height_mm, g_lift.max_height_mm);
}

static uint16_t lift_limit_to_u16(float x, uint16_t min_v)
{
	if (x <= (float)min_v)
	{
		return min_v;
	}
	if (x >= 65535.0f)
	{
		return 65535U;
	}
	return (uint16_t)x;
}

static void lift_send_traj_to_target(void)
{
	float dist_mm;
	float motor_sign;
	float move_rev;
	float move_deg;
	float vel_rpm;
	uint16_t acc_u16;
	uint16_t dec_u16;
	uint8_t dir;

	if (!g_lift.motor_enabled)
	{
		X_V2_En_Control(LIFT_MOTOR_ID, true, false);
		g_lift.motor_enabled = 1;
	}

	dist_mm = g_lift.target_height_mm - g_lift.height_mm;
	if (lift_absf(dist_mm) < 0.2f)
	{
		g_lift.motion_active = 0;
		g_lift.target_dirty = 0;
		g_lift.speed_rpm = 0.0f;
		X_V2_Stop_Now(LIFT_MOTOR_ID, false);
		return;
	}

	motor_sign = dist_mm * (float)g_lift.cmd_dir_sign;
	dir = (motor_sign >= 0.0f) ? 0U : 1U;

	if (g_lift.screw_pitch_mm < 1e-6f)
	{
		g_lift.motion_active = 0;
		g_lift.target_dirty = 0;
		X_V2_Stop_Now(LIFT_MOTOR_ID, false);
		return;
	}

	move_rev = lift_absf(dist_mm) / g_lift.screw_pitch_mm;
	move_deg = move_rev * 360.0f;
	vel_rpm = lift_clampf(g_lift.max_vel_rpm, 0.1f, 3000.0f);
	acc_u16 = lift_limit_to_u16(g_lift.acc_rpmps, 1U);
	dec_u16 = lift_limit_to_u16(g_lift.dec_rpmps, 1U);

	X_V2_Traj_Pos_Control(LIFT_MOTOR_ID, dir, acc_u16, dec_u16, vel_rpm, move_deg, 2U, false);

	g_lift.motion_active = 1;
	g_lift.target_dirty = 0;
}

void lift_init(void)
{
	lift_init_with_config((const lift_config_t *)0);
}

void lift_init_with_config(const lift_config_t *config)
{
	g_lift.height_mm = 0.0f;
	g_lift.speed_rpm = 0.0f;
	g_lift.target_height_mm = 0.0f;
	g_lift.zero_bias_mm = 0.0f;
	g_lift.motor_pos_rev = 0.0f;
	g_lift.screw_pitch_mm = LIFT_DEFAULT_PITCH_MM;
	g_lift.min_height_mm = LIFT_DEFAULT_MIN_MM;
	g_lift.max_height_mm = LIFT_DEFAULT_MAX_MM;
	g_lift.max_vel_rpm = LIFT_DEFAULT_MAX_RPM;
	g_lift.acc_rpmps = LIFT_DEFAULT_ACC_RPMPS;
	g_lift.dec_rpmps = LIFT_DEFAULT_DEC_RPMPS;
	g_lift.cmd_dir_sign = LIFT_DEFAULT_DIR_SIGN;
	g_lift.motor_enabled = 0;
	g_lift.has_feedback = 0;
	g_lift.feedback_req_div = 0;
	g_lift.target_dirty = 0;
	g_lift.motion_active = 0;
	g_lift.test_state = LIFT_TEST_IDLE;
	g_lift.test_down_mm = 0.0f;
	g_lift.test_up_mm = 0.0f;

	if (config != 0)
	{
		if (config->screw_pitch_mm > 0.1f)
		{
			g_lift.screw_pitch_mm = config->screw_pitch_mm;
		}

		g_lift.min_height_mm = config->min_height_mm;
		g_lift.max_height_mm = config->max_height_mm;
		if (g_lift.max_height_mm < g_lift.min_height_mm)
		{
			float tmp = g_lift.min_height_mm;
			g_lift.min_height_mm = g_lift.max_height_mm;
			g_lift.max_height_mm = tmp;
		}

		g_lift.max_vel_rpm = lift_absf(config->max_vel_rpm);
		g_lift.acc_rpmps = lift_absf(config->acc_rpmps);
		g_lift.dec_rpmps = lift_absf(config->dec_rpmps);
		g_lift.cmd_dir_sign = (config->cmd_dir_sign >= 0) ? 1 : -1;
	}

	X_V2_En_Control(LIFT_MOTOR_ID, true, false);
	g_lift.motor_enabled = 1;
	X_V2_Stop_Now(LIFT_MOTOR_ID, false);
}

void lift_enable(bool enable)
{
	X_V2_En_Control(LIFT_MOTOR_ID, enable, false);
	g_lift.motor_enabled = enable ? 1U : 0U;
	if (!enable)
	{
		g_lift.motion_active = 0;
		g_lift.speed_rpm = 0.0f;
	}
}

void lift_set_zero_height(float current_height_mm)
{
	g_lift.zero_bias_mm = current_height_mm - g_lift.motor_pos_rev * g_lift.screw_pitch_mm;
	lift_refresh_height_from_feedback();
	g_lift.target_height_mm = g_lift.height_mm;
	g_lift.target_dirty = 0;
	g_lift.motion_active = 0;
}

void lift_define_current_as_zero(void)
{
	lift_set_zero_height(0.0f);
}

void lift_set_height_limit(float min_height_mm, float max_height_mm)
{
	g_lift.min_height_mm = min_height_mm;
	g_lift.max_height_mm = max_height_mm;
	if (g_lift.max_height_mm < g_lift.min_height_mm)
	{
		float tmp = g_lift.min_height_mm;
		g_lift.min_height_mm = g_lift.max_height_mm;
		g_lift.max_height_mm = tmp;
	}

	g_lift.height_mm = lift_clampf(g_lift.height_mm, g_lift.min_height_mm, g_lift.max_height_mm);
	g_lift.target_height_mm = lift_clampf(g_lift.target_height_mm, g_lift.min_height_mm, g_lift.max_height_mm);
}

void lift_set_motion_limit(float max_vel_rpm, float acc_rpmps, float dec_rpmps)
{
	g_lift.max_vel_rpm = lift_absf(max_vel_rpm);
	g_lift.acc_rpmps = lift_absf(acc_rpmps);
	g_lift.dec_rpmps = lift_absf(dec_rpmps);
}

void lift_set_target_height(float target_height_mm)
{
	g_lift.target_height_mm = lift_clampf(target_height_mm, g_lift.min_height_mm, g_lift.max_height_mm);
	g_lift.target_dirty = 1U;
	lift_send_traj_to_target();
}

static void lift_send_relative_cmd(float delta_mm)
{
	float motor_sign;
	float move_rev;
	float move_deg;
	float vel_rpm;
	uint16_t acc_u16;
	uint16_t dec_u16;
	uint8_t dir;

	if (!g_lift.motor_enabled)
	{
		X_V2_En_Control(LIFT_MOTOR_ID, true, false);
		g_lift.motor_enabled = 1;
	}

	if (lift_absf(delta_mm) < 0.2f)
	{
		g_lift.motion_active = 0;
		g_lift.target_dirty = 0;
		g_lift.speed_rpm = 0.0f;
		X_V2_Stop_Now(LIFT_MOTOR_ID, false);
		return;
	}

	motor_sign = delta_mm * (float)g_lift.cmd_dir_sign;
	dir = (motor_sign >= 0.0f) ? 0U : 1U;

	if (g_lift.screw_pitch_mm < 1e-6f)
	{
		g_lift.motion_active = 0;
		g_lift.target_dirty = 0;
		X_V2_Stop_Now(LIFT_MOTOR_ID, false);
		return;
	}

	move_rev = lift_absf(delta_mm) / g_lift.screw_pitch_mm;
	move_deg = move_rev * 360.0f;
	vel_rpm = lift_clampf(g_lift.max_vel_rpm, 0.1f, 3000.0f);
	acc_u16 = lift_limit_to_u16(g_lift.acc_rpmps, 1U);
	dec_u16 = lift_limit_to_u16(g_lift.dec_rpmps, 1U);

	X_V2_Traj_Pos_Control(LIFT_MOTOR_ID, dir, acc_u16, dec_u16, vel_rpm, move_deg, 2U, false);

	g_lift.target_height_mm = g_lift.height_mm + delta_mm;
	g_lift.motion_active = 1;
	g_lift.target_dirty = 0;
}

void lift_move_up_mm(float delta_mm)
{
	float d = lift_absf(delta_mm);
	uint32_t wait_start;

	lift_send_relative_cmd(-d);

#ifdef CMSIS_OS2_H_
	osDelay(300);
#else
	HAL_Delay(300); /* 等待电机实际启动 */
#endif

	wait_start = HAL_GetTick();
	do
	{
		X_V2_Read_Sys_Params(LIFT_MOTOR_ID, S_VEL);
#ifdef CMSIS_OS2_H_
		osDelay(20);
#else
		HAL_Delay(20); /* 等待 CAN 回包触发中断更新 speed_rpm */
#endif
		if (HAL_GetTick() - wait_start > 10000)
		{
			break;
		}
	} while (lift_absf(g_lift.speed_rpm) > 2.0f);
}

void lift_move_down_mm(float delta_mm)
{
	float d = lift_absf(delta_mm);
	uint32_t wait_start;

	lift_send_relative_cmd(d);

#ifdef CMSIS_OS2_H_
	osDelay(300);
#else
	HAL_Delay(300); /* 等待电机实际启动 */
#endif

	wait_start = HAL_GetTick();
	do
	{
		X_V2_Read_Sys_Params(LIFT_MOTOR_ID, S_VEL);
#ifdef CMSIS_OS2_H_
		osDelay(20);
#else
		HAL_Delay(20); /* 等待 CAN 回包触发中断更新 speed_rpm */
#endif
		if (HAL_GetTick() - wait_start > 10000)
		{
			break;
		}
	} while (lift_absf(g_lift.speed_rpm) > 2.0f);
}

void lift_force_move_up_mm(float delta_mm)
{
	float d = lift_absf(delta_mm);
	lift_send_relative_cmd(-d);
}

void lift_start_down_up_test(float down_mm, float up_mm)
{
	g_lift.test_down_mm = lift_absf(down_mm);
	g_lift.test_up_mm = lift_absf(up_mm);
	lift_move_down_mm(g_lift.test_down_mm);
	g_lift.test_state = LIFT_TEST_MOVING_DOWN;
}

bool lift_is_test_done(void)
{
	return (g_lift.test_state == LIFT_TEST_DONE);
}

void lift_update(float dt_s)
{
	if (dt_s <= 0.0f)
	{
		dt_s = 0.001f;
	}

	g_lift.feedback_req_div++;
	if (g_lift.feedback_req_div >= LIFT_FEEDBACK_REQ_DIV)
	{
		g_lift.feedback_req_div = 0U;
		X_V2_Read_Sys_Params(LIFT_MOTOR_ID, S_CPOS);
		X_V2_Read_Sys_Params(LIFT_MOTOR_ID, S_VEL);
	}

	if (g_lift.motion_active)
	{
		float dist_mm = g_lift.target_height_mm - g_lift.height_mm;
		/* 只有在有反馈且满足条件，或者使用测试状态机超时机制时才取消 active */
		if (lift_absf(dist_mm) <= 0.5f && lift_absf(g_lift.speed_rpm) <= 2.0f && lift_has_feedback())
		{
			g_lift.motion_active = 0;
		}
	}

	switch ((lift_test_state_t)g_lift.test_state)
	{
		case LIFT_TEST_MOVING_DOWN:
			if (lift_is_target_reached(1.0f))
			{
				lift_move_up_mm(g_lift.test_up_mm);
				g_lift.test_state = LIFT_TEST_MOVING_UP;
			}
			break;

		case LIFT_TEST_MOVING_UP:
			if (lift_is_target_reached(1.0f))
			{
				lift_stop();
				g_lift.test_state = LIFT_TEST_DONE;
			}
			break;

		case LIFT_TEST_IDLE:
		case LIFT_TEST_DONE:
		default:
			break;
	}
}

void lift_stop(void)
{
	g_lift.target_height_mm = g_lift.height_mm;
	g_lift.speed_rpm = 0.0f;
	g_lift.target_dirty = 0;
	g_lift.motion_active = 0;
	X_V2_Stop_Now(LIFT_MOTOR_ID, false);
}

bool lift_is_target_reached(float tol_mm)
{
	float tol;

	tol = (tol_mm > 0.0f) ? tol_mm : 0.5f;
	return (lift_absf(g_lift.target_height_mm - g_lift.height_mm) <= tol) && (lift_absf(g_lift.speed_rpm) <= 2.0f);
}

bool lift_has_feedback(void)
{
	return (g_lift.has_feedback != 0U);
}

float lift_get_height_mm(void)
{
	return g_lift.height_mm;
}

float lift_get_target_height_mm(void)
{
	return g_lift.target_height_mm;
}

float lift_get_speed_rpm(void)
{
	return g_lift.speed_rpm;
}

void lift_can_parse_xv2_feedback(uint32_t ext_id, const uint8_t *rx_data, uint8_t dlc)
{
	uint8_t motor_addr;

	if (rx_data == (const uint8_t *)0)
	{
		return;
	}

	motor_addr = (uint8_t)((ext_id >> 8) & 0xFFU);
	if (motor_addr != LIFT_MOTOR_ID)
	{
		return;
	}

	/* S_VEL(0x35) 回包与四轮底盘一致: data[0]=0x35, data[1]=方向(0=CW,1=CCW),
	   data[2..3]=转速幅值(大端, 无符号, 单位 0.1RPM)。两个易错点:
	   1) 单位是 0.1RPM(与速度指令 vel*10 一致), 必须 /10 才是 RPM, 否则幅值放大10倍;
	   2) 幅值是"无符号+独立方向字节", 按有符号16位拼会把方向字节当幅值高字节,
	      CW(方向=0) 恒读成 0 → 升降等待循环误判为已停。 */
	if (dlc >= 4U && rx_data[0] == 0x35U)
	{
		uint8_t dir = rx_data[1];
		uint16_t mag = (uint16_t)(((uint16_t)rx_data[2] << 8) | (uint16_t)rx_data[3]);
		int16_t speed_raw = (dir != 0U) ? (int16_t)(mag / 10) : -(int16_t)(mag / 10);
		g_lift.speed_rpm = (float)speed_raw * (float)g_lift.cmd_dir_sign;
		g_lift.has_feedback = 1U;
		return;
	}

	if (dlc >= 5U && rx_data[0] == 0x36U)
	{
		int32_t pos_raw = ((int32_t)rx_data[1] << 24)
			| ((int32_t)rx_data[2] << 16)
			| ((int32_t)rx_data[3] << 8)
			| (int32_t)rx_data[4];
		g_lift.motor_pos_rev = ((float)pos_raw / 10.0f) / 360.0f * (float)g_lift.cmd_dir_sign;
		lift_refresh_height_from_feedback();
		g_lift.has_feedback = 1U;
	}
}
