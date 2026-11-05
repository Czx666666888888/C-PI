#include "chassis.h"

#include <math.h>

#include "X_V2.h"
#include "chassis_can.h"
#include "BMI088driver.h"
#include "BMI088Middleware.h"

/* 塔吊模式标志: 1=底盘任务不再占用 CAN 总线(由 TOWER_Task 独占总线) */
extern volatile uint8_t g_tower_mode_active;

#define CHASSIS_WHEEL_RADIUS_M      (0.0375f)
#define CHASSIS_HALF_LENGTH_M       (0.22075f * 0.5f)
#define CHASSIS_HALF_WIDTH_M        (0.156f * 0.5f)
#define CHASSIS_GEAR_RATIO          (1.0f)

#define CHASSIS_MOTOR_ACC_RPMPS     (800U)
#define CHASSIS_XV2_MAX_RPM         (3000.0f)
/* 速度下发周期(ms): 广播速度与读回包分时, 每这么久广播一拍速度, 其余拍轮流读回包。 */
#define CHASSIS_VEL_PERIOD_MS       (10U)
/* 底盘速度上限: 单轮最大转速(RPM)。直线平移时四轮期望转速即车身速度上限,
 * 原地旋转/复合运动时该值作为最外侧轮速的硬钳位。 */
#define CHASSIS_DEFAULT_MAX_WHEEL_RPM (800.0f)
/* 闭环模式最大电流(扭矩上限), 单位 mA, 驱动协议范围 0~6000。
 * 已提到驱动上限 6000mA 以获得最大扭矩(横移高负载时更稳)。
 * 注意: 电机额定 5000mA, 6000 已超额定 20%, 长时间满扭矩会发热,
 * 建议留意电机温度(S_TEMP 回包), 需要保守时改回 5000。 */
#define CHASSIS_MOTOR_MAX_CURRENT_MA (6000U)
#define CHASSIS_PI                  (3.14159265358979323846f)

/* 单轮速度一致性(离群钳位): 实测偏离"自己目标"超过该死区(RPM)时, 在 PI 之外额外
 * 按比例强力纠偏(跑飞→降、卡死→补)。基准用"目标"而非"四轮平均", 因为横移时对角轮
 * 符号相反(平均≈0)、旋转时四轮目标本就不同, 跟平均比会反向拉回、破坏横移/旋转。 */
#define CHASSIS_WHEEL_OUTLIER_DEADBAND_RPM (40.0f)
#define CHASSIS_WHEEL_OUTLIER_KP           (0.0f)   /* 已关闭: 与单轮PI双重纠偏导致抖动 */

/* 陀螺仪 Z 轴到底盘 wz 的符号:
 * 底盘约定 wz>0 为逆时针(俯视)。若底盘前进时偏航修正方向反了(越修越偏),
 * 把下面改为 -1.0f 即可。 */
#define CHASSIS_GYRO_WZ_SIGN        (1.0f)

/* 自转中心保持: 位置误差死区(m), 误差小于该值不补偿, 避免原地旋转时抖动 */
#define CHASSIS_HOLD_POS_DEADZONE   (0.01f)

/* 1: 后轮反向(默认)；0: 四轮同向，用于快速排查电机安装方向问题。 */
#ifndef CHASSIS_REVERSE_REAR_MOTORS
#define CHASSIS_REVERSE_REAR_MOTORS (1)
#endif

typedef struct
{
	float kp;
	float ki;
	float kd;
	float i_limit;
	float out_limit;
	float integral;
	float prev_err;
} chassis_pid_t;

static const uint8_t k_motor_ids[4] = {1, 2, 3, 4};
/* 坐标系约定: vx前进, vy左移, wz逆时针。轮序: 前左 前右 后左 后右。 */
#if CHASSIS_REVERSE_REAR_MOTORS
static const int8_t k_motor_cmd_dir_sign[4] = {1, -1, 1, -1};  // FL(+), FR(-), RL(+), RR(-): 左右两侧镜像安装
/* 回包方向符号 = 指令方向符号 的相反数。X_V2 转速回包(S_VEL)的符号约定与速度指令
 * dir 位相反: 指令 dir=1(CCW) 时回包为正。若不取反, 单轮速度闭环会变成正反馈,
 * 积分失控飙升 → 横移时四轮指令大小/方向错乱、车斜飞(实测 M 与目标 T 反号)。 */
static const int8_t k_motor_feedback_dir_sign[4] = {-1, 1, -1, 1};
#else
static const int8_t k_motor_cmd_dir_sign[4] = {1, 1, 1, 1};
static const int8_t k_motor_feedback_dir_sign[4] = {-1, -1, -1, -1};
#endif
/* 速度补偿倍率：如果某个轮子明显偏慢，可以先只改这里，不动解算公式。
 * 之前给后轮(3/4号)1.25 是误判——当时"后轮不转"实为航向保持 wz 吃掉了后轮横移目标,
 * 并非后轮负载大。现在数据(前轮 PI 修正为正、后轮为负)表明质心偏前、前轮负载略大,
 * 故撤回后轮倍率, 四轮回到 1.0, 负载差异交给单轮 PI 闭环精调。 */
static const float k_motor_rpm_scale[4] = {1.0f, 1.0f, 1.0f, 1.0f};

/* 最近一次下发的底盘坐标系目标轮速(RPM), 供 OLED 诊断与实测值对比 */
static float g_last_cmd_wheel_rpm[4] = {0.0f, 0.0f, 0.0f, 0.0f};

static float g_target_vx = 0.0f;
static float g_target_vy = 0.0f;
static float g_target_wz = 0.0f;

static float g_max_vx = 1.2f;
static float g_max_vy = 1.2f;
static float g_max_wz = 4.0f;
static float g_max_wheel_rpm = CHASSIS_DEFAULT_MAX_WHEEL_RPM;  /* 底盘速度上限(单轮 RPM) */

static bool g_pid_enable = true;
static bool g_motor_enabled = false;

/* 陀螺仪偏航闭环状态 */
static bool  g_gyro_yaw_enable = false;   /* 使能后 wz 反馈改由陀螺仪测量真实角速度 */
static float g_gyro_wz_bias = 0.0f;       /* 陀螺仪 Z 轴零偏(静止时标定) */

/* 航向保持(角度闭环): 直线/平移时保持偏航角恒定, 消除重心偏移导致的累计偏航。
 * 纯 P 控制有稳态偏航角残留(横移四轮速度差产生的 wz 扰动 → 车停在一个平衡偏角上
 * error=扰动/kp 压不回零), 故加积分项把稳态偏角归零。 */
static float g_heading = 0.0f;           /* 当前偏航角(陀螺积分), rad, 逆时针为正 */
static float g_target_heading = 0.0f;    /* 航向保持目标角, rad */
static float g_heading_kp = 3.0f;        /* 航向保持 P 增益(可调) */
static float g_heading_ki = 0.8f;        /* 航向保持 I 增益(消除横移稳态偏航角, 不宜过大) */
static float g_heading_integral = 0.0f;  /* 航向积分累计 */
static float g_heading_i_limit = 0.6f;   /* 航向积分限幅(rad/s 量级, 防风up) */
static float g_heading_wz_limit = 0.0f;  /* 航向保持 wz 输出限幅(rad/s), 平移时彻底禁用航向保持 */

/* 自转中心保持(位置闭环): 纯原地旋转时锁定世界坐标, 消除轮速不对称/质心偏移导致的旋转漂移 */
static float g_pos_hold_kp = 1.5f;       /* 位置误差→速度 的 P 增益(可调) */
static float g_hold_x = 0.0f;            /* 旋转起点世界坐标 X(锁存) */
static float g_hold_y = 0.0f;            /* 旋转起点世界坐标 Y(锁存) */

/* 指令边沿检测: 遥控任务每周期都调 set_target, 必须只在"进入"平移/纯旋转的
   瞬间锁存目标, 否则锁存值会被持续覆盖导致闭环失效 */
static bool g_prev_translating = false;  /* 上一次指令是否处于平移态 */
static bool g_prev_rotating    = false;  /* 上一次指令是否处于纯旋转态 */
static bool g_pure_rotate      = false;  /* 当前指令是否为"纯原地旋转"(无平移分量) */

/* 位置控制状态 */
static float g_pos_x = 0.0f;
static float g_pos_y = 0.0f;
static float g_pos_theta = 0.0f;

static float g_target_pos_x = 0.0f;
static float g_target_pos_y = 0.0f;
static float g_target_pos_theta = 0.0f;

static bool  g_position_ctrl_enable = false;
static float g_pos_kp_xy    = 1.5f;
static float g_pos_kp_theta = 2.5f;

/* 车身速度环(外环): 目标速度 → 修正后的车身速度指令。反馈为四轮编码器换算的车身速度。
 * 横移(vy)时 vx 目标=0, 四轮负载不均会让轮速差漏出 vx 分量(斜飞), 本环负责把它压回 0;
 * 偏转(ω)则由 wz 环 + 航向保持共同抑制。之前反馈被放大10倍(未/10)时高增益会低频振荡,
 * 反馈修正正确后可恢复增益: kp=0.4 快速抑制 vx/vy 泄漏, wz 环 kp=0.5。
 * 单轮 PI 负责四轮稳态拉平, 本环只做外层快速修正, 不加积分(避免与单轮 PI 积分叠加振荡)。 */
static chassis_pid_t g_pid_vx = {0.4f, 0.0f, 0.0f, 0.5f, 0.6f, 0.0f, 0.0f};
static chassis_pid_t g_pid_vy = {0.4f, 0.0f, 0.0f, 0.5f, 0.6f, 0.0f, 0.0f};
static chassis_pid_t g_pid_wz = {0.5f, 0.0f, 0.0f, 1.0f, 1.2f, 0.0f, 0.0f};

/* 单轮速度闭环(编码器): 叠加在电机自带 FOC 速度环之上, 把四轮实际转速拉到各自目标,
 * 输出为轮速修正量(RPM)。结构: 指令 = 期望轮速(前馈) + PI(期望-实测)。
 * 反馈(0.1RPM→RPM)修正正确后, 横移斜飞/偏转的根因是"轮间负载差异 → 四轮速度不一致(稳态)",
 * 这正是本环要消除的:
 *   - kp=0.5: 比例快速响应瞬态偏差;
 *   - ki=1.0: 积分消除负载造成的稳态速度差。后轮负载大, 已由 k_motor_rpm_scale=1.25
 *     前馈粗补掉大部分, 闭环只做精调, 故 ki 不必过大——ki=2.0 时低速反馈噪声被积分
 *     累积放大 → 车身明显抖动, 降到 1.0 平滑低速。
 * 之前"一卡一卡"误归咎于 ki, 实为反馈被放大10倍所致; 反馈正确后 ki 安全。
 * 停车(死区)/换向均清零积分, 防止起步窜动与换向冲击。 */
static chassis_pid_t g_pid_wheel[4] = {
    {0.5f, 1.0f, 0.0f, 150.0f, 300.0f, 0.0f, 0.0f},
    {0.5f, 1.0f, 0.0f, 150.0f, 300.0f, 0.0f, 0.0f},
    {0.5f, 1.0f, 0.0f, 150.0f, 300.0f, 0.0f, 0.0f},
    {0.5f, 1.0f, 0.0f, 150.0f, 300.0f, 0.0f, 0.0f},
};

static float clampf(float x, float min_v, float max_v)
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

static float absf(float x)
{
	return (x >= 0.0f) ? x : -x;
}

static float wrap_pi(float a)
{
	a = fmodf(a, 2.0f * CHASSIS_PI);
	if (a >  CHASSIS_PI) { a -= 2.0f * CHASSIS_PI; }
	if (a < -CHASSIS_PI) { a += 2.0f * CHASSIS_PI; }
	return a;
}

static float pid_step(chassis_pid_t *pid, float target, float measure, float dt)
{
	float err = target - measure;
	float derivative;
	float out;

	if (dt <= 0.0f)
	{
		dt = 0.001f;
	}

	pid->integral += err * dt;
	pid->integral = clampf(pid->integral, -pid->i_limit, pid->i_limit);

	derivative = (err - pid->prev_err) / dt;
	pid->prev_err = err;

	out = pid->kp * err + pid->ki * pid->integral + pid->kd * derivative;
	return clampf(out, -pid->out_limit, pid->out_limit);
}

static bool read_feedback_body_velocity(float *vx, float *vy, float *wz)
{
	float w[4];
	float k = CHASSIS_HALF_LENGTH_M + CHASSIS_HALF_WIDTH_M;
	uint8_t i;

	for (i = 0; i < 4; i++)
	{
		if (g_chassis_motors[i].can_id == 0U)
		{
			*vx = 0.0f;
			*vy = 0.0f;
			*wz = 0.0f;
			return false;
		}

		/* 假设回传速度单位为 RPM。先转轮速(rad/s)，再根据左右轮方向映射。 */
		w[i] = ((float)g_chassis_motors[i].rotor_speed) * (2.0f * CHASSIS_PI / 60.0f) / CHASSIS_GEAR_RATIO;
		w[i] *= (float)k_motor_feedback_dir_sign[i];
	}

	*vx = CHASSIS_WHEEL_RADIUS_M * (w[0] + w[1] + w[2] + w[3]) * 0.25f;
	*vy = CHASSIS_WHEEL_RADIUS_M * (-w[0] + w[1] + w[2] - w[3]) * 0.25f;
	*wz = CHASSIS_WHEEL_RADIUS_M * (-w[0] + w[1] - w[2] + w[3]) / (4.0f * k);
	return true;
}

static void body_to_wheel_omega(float vx, float vy, float wz, float wheel_omega[4])
{
	float k = CHASSIS_HALF_LENGTH_M + CHASSIS_HALF_WIDTH_M;
	float inv_r = 1.0f / CHASSIS_WHEEL_RADIUS_M;

	/* 轮序: 前左 前右 后左 后右 */
	wheel_omega[0] = (vx - vy - k * wz) * inv_r;
	wheel_omega[1] = (vx + vy + k * wz) * inv_r;
	wheel_omega[2] = (vx + vy - k * wz) * inv_r;
	wheel_omega[3] = (vx - vy + k * wz) * inv_r;
}

static float measured_wheel_rpm(uint8_t i)
{
	/* 电机回传 rotor_speed 为电机轴 RPM(电机自身坐标系)。除以减速比得轮子 RPM,
	   再乘 feedback 方向符号统一到底盘坐标系, 与目标轮速(target)同坐标系。 */
	return ((float)g_chassis_motors[i].rotor_speed) / CHASSIS_GEAR_RATIO * (float)k_motor_feedback_dir_sign[i];
}

void chassis_get_wheel_rpm(float wheel_rpm[4])
{
	/* 供 OLED 诊断用: 返回四轮实测转速(底盘坐标系, 带符号)。轮序: 前左 前右 后左 后右。 */
	uint8_t i;

	for (i = 0; i < 4; i++)
	{
		wheel_rpm[i] = measured_wheel_rpm(i);
	}
}

void chassis_get_wheel_cmd_rpm(float wheel_rpm[4])
{
	/* 供 OLED 诊断用: 返回最近一次下发的底盘坐标系目标轮速(带符号), 与实测值对比,
	   可区分"代码没下发(目标≈0)"还是"下发了但电机不转(目标≠0、实测=0)"。 */
	uint8_t i;

	for (i = 0; i < 4; i++)
	{
		wheel_rpm[i] = g_last_cmd_wheel_rpm[i];
	}
}

static void apply_wheel_speed_closed_loop(float wheel_omega[4], float dt_s)
{
	uint8_t i;
	/* 记录每轮上一拍目标轮速, 检测换向(前进↔后退 / 左移↔右移)。
	   换向瞬间旧方向累积的负载补偿积分若不清零, 会反向推着轮子多转 → 车身"懵懂一震"。 */
	static float last_target_rpm[4] = {0.0f, 0.0f, 0.0f, 0.0f};

	for (i = 0; i < 4; i++)
	{
		float target_rpm = wheel_omega[i] * 60.0f / (2.0f * CHASSIS_PI);
		float meas_rpm   = measured_wheel_rpm(i);
		float corr_rpm;

		/* 目标符号翻转(乘积<0): 清零该轮积分与误差历史, 避免换向冲击 */
		if (last_target_rpm[i] * target_rpm < 0.0f)
		{
			g_pid_wheel[i].integral = 0.0f;
			g_pid_wheel[i].prev_err = 0.0f;
		}
		last_target_rpm[i] = target_rpm;

		corr_rpm = pid_step(&g_pid_wheel[i], target_rpm, meas_rpm, dt_s);
		wheel_omega[i] += corr_rpm * (2.0f * CHASSIS_PI / 60.0f);
	}
}

static void apply_wheel_speed_outlier_correction(float wheel_omega[4])
{
	uint8_t i;

	for (i = 0; i < 4; i++)
	{
		float target_rpm = wheel_omega[i] * 60.0f / (2.0f * CHASSIS_PI);
		float meas_rpm   = measured_wheel_rpm(i);
		float err        = target_rpm - meas_rpm;
		float excess     = 0.0f;

		if (err > CHASSIS_WHEEL_OUTLIER_DEADBAND_RPM)
		{
			excess = err - CHASSIS_WHEEL_OUTLIER_DEADBAND_RPM;
		}
		else if (err < -CHASSIS_WHEEL_OUTLIER_DEADBAND_RPM)
		{
			excess = err + CHASSIS_WHEEL_OUTLIER_DEADBAND_RPM;
		}

		wheel_omega[i] += CHASSIS_WHEEL_OUTLIER_KP * excess * (2.0f * CHASSIS_PI / 60.0f);
	}
}

static void send_wheel_rpm_broadcast(const float wheel_omega[4])
{
	uint8_t i;

	/* 记录本次计算的底盘坐标系目标轮速(RPM), 供 OLED 诊断与实测值对比 */
	for (i = 0; i < 4; i++)
	{
		g_last_cmd_wheel_rpm[i] = wheel_omega[i] * 60.0f / (2.0f * CHASSIS_PI);
	}

	/* 多机广播下发速度: 4 个电机指令先拼进 MMCL_cmd[], 再一次性 X_V2_Multi_Motor_Cmd(0x00)
	   广播出去。广播帧 ExtId 0x000-0x005 与单电机读回包 ExtId 0x100-0x400 完全不同,
	   不会同 ID 撞帧; 且广播与读在调度里分时(广播的拍不读、读的拍不广播),
	   广播的高优先级 0x000 也不会抢掉任何电机的回包。
	   注意 snF 传 false(立即执行): 传 true 是"同步运动", 电机要等触发才转。 */
	for (i = 0; i < 4; i++)
	{
		float wheel_rpm = wheel_omega[i] * 60.0f / (2.0f * CHASSIS_PI);
		float motor_rpm = wheel_rpm * k_motor_rpm_scale[i] * CHASSIS_GEAR_RATIO * (float)k_motor_cmd_dir_sign[i];
		uint8_t dir = (motor_rpm >= 0.0f) ? 0U : 1U;
		float rpm_abs = absf(motor_rpm);

		rpm_abs = clampf(rpm_abs, 0.0f, CHASSIS_XV2_MAX_RPM);

		X_V2_MMCL_Vel_Control(k_motor_ids[i], dir, CHASSIS_MOTOR_ACC_RPMPS, rpm_abs, false);
	}
	X_V2_Multi_Motor_Cmd(0x00);
}

/* 发送调度: 广播速度与读回包二选一, 绝不同拍。
   每 CHASSIS_VEL_PERIOD_MS 拍广播一次速度, 其余拍轮流读一个电机(1→2→3→4)。
   广播帧 0x000 是总线最高优先级, 若与读回包同拍会把回包抢掉; 分时后两者彻底错开。 */
static void chassis_tx_schedule(const float wheel_omega[4])
{
	static uint32_t last_vel_tick = 0U;
	static uint8_t rr_idx = 0U;
	uint32_t now = HAL_GetTick();

	if ((now - last_vel_tick) >= CHASSIS_VEL_PERIOD_MS)
	{
		last_vel_tick = now;
		send_wheel_rpm_broadcast(wheel_omega);
	}
	else
	{
		X_V2_Read_Sys_Params(k_motor_ids[rr_idx], S_VEL);
		rr_idx = (rr_idx + 1U) & 3U;
	}
}

void chassis_init(void)
{
	uint8_t i;

	g_target_vx = 0.0f;
	g_target_vy = 0.0f;
	g_target_wz = 0.0f;

	/* 给 CAN 总线一点时间完成初始化和同步，防止使能指令在总线未就绪时丢失 */
#ifdef CMSIS_OS2_H_
	osDelay(500);
#else
	HAL_Delay(500);
#endif

	for (i = 0; i < 4; i++)
	{
		X_V2_En_Control(k_motor_ids[i], true, false);
		/* 确保闭环 FOC 模式(0=开环步进, 1=闭环FOC)。开环模式下电机无速度/扭矩闭环,
		   负载稍重就丢步卡顿, 且 FOC_mA 电流上限也不生效。svF=false 不写 Flash。 */
		X_V2_Modify_Ctrl_Mode(k_motor_ids[i], false, true);
		/* 提高闭环模式最大电流(扭矩上限), 否则负载稍重就"带不动"。
		   svF=false 不写入电机 Flash, 每次上电由本函数重新下发。 */
		X_V2_Modify_FOC_mA(k_motor_ids[i], false, CHASSIS_MOTOR_MAX_CURRENT_MA);
	}
	g_motor_enabled = true;
}

void chassis_gyro_calibrate(void)
{
    fp32 gyro[3];
    float sum = 0.0f;
    const int samples = 200;
    int i;

    /* 车辆静止时多次采样求平均, 得到陀螺仪 Z 轴零偏 */
    for (i = 0; i < samples; i++)
    {
        BMI088_gyro_read(gyro);
        sum += gyro[2];
        BMI088_delay_ms(1);
    }
    g_gyro_wz_bias = sum / (float)samples;
}

void chassis_set_gyro_yaw_enable(bool enable)
{
    g_gyro_yaw_enable = enable;
}

void chassis_set_heading_hold_gain(float kp)
{
    g_heading_kp = absf(kp);
}

void chassis_set_rotation_hold_gain(float kp)
{
    g_pos_hold_kp = absf(kp);
}

void chassis_set_target(float vx_mps, float vy_mps, float wz_radps)
{
	bool translating;

	g_target_vx = clampf(vx_mps, -g_max_vx, g_max_vx);
	g_target_vy = clampf(vy_mps, -g_max_vy, g_max_vy);
	g_target_wz = clampf(wz_radps, -g_max_wz, g_max_wz);

	/* 平移: 有前后/左右分量且几乎不自转；纯旋转: 只有自转分量, 无平移分量。
	   区分二者, 避免"边转边平移"时误触发航向保持或自转中心保持。 */
	translating    = (absf(g_target_wz) < 0.01f) &&
	                 (absf(g_target_vx) > 0.01f || absf(g_target_vy) > 0.01f);
	g_pure_rotate  = (absf(g_target_wz) >= 0.01f) &&
	                 (absf(g_target_vx) < 0.01f) && (absf(g_target_vy) < 0.01f);

	/* 航向保持: 仅在"进入平移"的瞬间锁存当前航向(边沿触发)。
	   原先每次调用都覆盖 g_target_heading, 使航向误差恒为0, 航向保持实际失效。
	   进入平移瞬间同时清零航向积分, 避免上个运动态残留的积分在本次平移方向不对 → 冲击。 */
	if (translating && !g_prev_translating)
	{
		g_target_heading = g_heading;
		g_heading_integral = 0.0f;
	}
	g_prev_translating = translating;

	/* 自转中心保持: 仅在"进入纯旋转"的瞬间锁存当前世界坐标 */
	if (g_pure_rotate && !g_prev_rotating)
	{
		g_hold_x = g_pos_x;
		g_hold_y = g_pos_y;
	}
	g_prev_rotating = g_pure_rotate;
}

void chassis_set_max_motion(float max_vx_mps, float max_vy_mps, float max_wz_radps, float max_wheel_rpm)
{
	g_max_vx = absf(max_vx_mps);
	g_max_vy = absf(max_vy_mps);
	g_max_wz = absf(max_wz_radps);
	g_max_wheel_rpm = absf(max_wheel_rpm);
}

void chassis_set_pid_enable(bool enable)
{
	g_pid_enable = enable;
}

void chassis_set_pid_gains(const chassis_pid_gain_t *vx_gain, const chassis_pid_gain_t *vy_gain, const chassis_pid_gain_t *wz_gain)
{
	if (vx_gain != 0)
	{
		g_pid_vx.kp = vx_gain->kp;
		g_pid_vx.ki = vx_gain->ki;
		g_pid_vx.kd = vx_gain->kd;
		g_pid_vx.i_limit = absf(vx_gain->i_limit);
		g_pid_vx.out_limit = absf(vx_gain->out_limit);
	}

	if (vy_gain != 0)
	{
		g_pid_vy.kp = vy_gain->kp;
		g_pid_vy.ki = vy_gain->ki;
		g_pid_vy.kd = vy_gain->kd;
		g_pid_vy.i_limit = absf(vy_gain->i_limit);
		g_pid_vy.out_limit = absf(vy_gain->out_limit);
	}

	if (wz_gain != 0)
	{
		g_pid_wz.kp = wz_gain->kp;
		g_pid_wz.ki = wz_gain->ki;
		g_pid_wz.kd = wz_gain->kd;
		g_pid_wz.i_limit = absf(wz_gain->i_limit);
		g_pid_wz.out_limit = absf(wz_gain->out_limit);
	}
}

void chassis_set_wheel_pid_gains(const chassis_pid_gain_t *gain)
{
	uint8_t i;

	if (gain == 0)
	{
		return;
	}

	for (i = 0; i < 4; i++)
	{
		g_pid_wheel[i].kp = gain->kp;
		g_pid_wheel[i].ki = gain->ki;
		g_pid_wheel[i].kd = gain->kd;
		g_pid_wheel[i].i_limit = absf(gain->i_limit);
		g_pid_wheel[i].out_limit = absf(gain->out_limit);
	}
}

void chassis_update(float dt_s)
{
	float vx_meas = 0.0f;
	float vy_meas = 0.0f;
	float wz_meas = 0.0f;
	float vx_cmd = g_target_vx;
	float vy_cmd = g_target_vy;
	float wz_cmd = g_target_wz;
	float gyro_wz;
	float wz_target;
	float wheel_omega[4];
	bool feedback_ok;
	float max_cmd_rpm;
	uint8_t i;

	if (g_tower_mode_active)
	{
		/* 塔吊模式: 底盘任务完全不碰 CAN(回包轮询+轮速指令), 让 TOWER_Task 独占总线 */
		return;
	}

	if (!g_motor_enabled)
	{
		/* 电机失能时仅尝试重新使能（不阻塞），
		   完整初始化 (含 500ms 延时等 CAN 稳定) 已在 Chassis_Task 启动时完成 */
		uint8_t i;
		for (i = 0; i < 4; i++)
		{
			X_V2_En_Control(k_motor_ids[i], true, false);
		}
		g_motor_enabled = true;
	}

	/* 读回包请求已在 chassis_tx_schedule 里轮流下发(1→2→3→4), 这里只从缓存取值。
	   若四轮连读, 电机1/2的高优先级回包(0x100/0x200)会抢占电机3/4的(0x300/0x400),
	   且电机丢仲裁后不重发 → 3/4 反馈丢失; 所以一次只读一个、且与广播速度分时。 */
	feedback_ok = read_feedback_body_velocity(&vx_meas, &vy_meas, &wz_meas);

	/* 陀螺仪偏航角积分(逆时针为正) */
	gyro_wz = CHASSIS_GYRO_WZ_SIGN * (g_imu_gyro[2] - g_gyro_wz_bias);
	g_heading = wrap_pi(g_heading + gyro_wz * dt_s);

	/* 里程计: 用陀螺航向 + 编码器机体系速度积分世界坐标, 供自转中心保持使用 */
	if (feedback_ok)
	{
		float cos_t = cosf(g_heading);
		float sin_t = sinf(g_heading);
		g_pos_x += (vx_meas * cos_t - vy_meas * sin_t) * dt_s;
		g_pos_y += (vx_meas * sin_t + vy_meas * cos_t) * dt_s;
	}

	/* 停止死区: 目标速度全为 0 时彻底停车, 跳过航向保持和 PID,
	   避免陀螺零偏导致车停住后仍缓慢自转/蠕动 */
	if (absf(g_target_vx) < 0.01f && absf(g_target_vy) < 0.01f && absf(g_target_wz) < 0.01f)
	{
		/* 停车时清零单轮 PI 积分: 防止横移/直行积累的积分残余, 在下次起步瞬间
		   推着某个轮子多转(停车不彻底/起步窜)。航向积分同理清零。 */
		for (i = 0; i < 4; i++)
		{
			g_pid_wheel[i].integral = 0.0f;
			g_pid_wheel[i].prev_err = 0.0f;
		}
		g_heading_integral = 0.0f;
		body_to_wheel_omega(0.0f, 0.0f, 0.0f, wheel_omega);
		chassis_tx_schedule(wheel_omega);
		return;
	}

	/* wz 目标: 直线/平移(wz≈0)时用航向保持(角度闭环), 旋转时用原始目标 */
	wz_target = g_target_wz;
	if (g_gyro_yaw_enable && absf(g_target_wz) < 0.01f)
	{
		/* PI 航向保持: P 快速纠偏, I 消除横移稳态偏航角(把车拉回锁存航向, 不残留偏角)。
		   wz 输出必须限幅(±g_heading_wz_limit): 麦轮横移时, wz 项在后轮上与 vy 项
		   一正一负相互抵消, 若 wz 过大(曾达 -1.92 rad/s)会把后轮横移目标吃到接近 0,
		   后轮不转→只剩前轮推横移→更偏转→wz 更大, 形成恶性循环。
		   实测: 只要航向保持还输出哪怕 0.05 rad/s 的 wz, 就会制造 ~2.4 RPM 前后轮差,
		   让车横移时划弧线(左移逆时针/右移顺时针)。故平移时彻底禁用(限 0), 四轮目标
		   纯对称, 偏航完全靠"四轮速度一致(单轮 PI 拉平)"来预防, 不再用 wz 硬掰。 */
		float heading_err = wrap_pi(g_target_heading - g_heading);
		g_heading_integral = clampf(g_heading_integral + heading_err * dt_s,
		                            -g_heading_i_limit, g_heading_i_limit);
		wz_target = clampf(g_heading_kp * heading_err + g_heading_ki * g_heading_integral,
		                   -g_heading_wz_limit, g_heading_wz_limit);
	}
	else if (absf(g_target_wz) >= 0.01f)
	{
		/* 纯旋转/复合旋转态: 航向保持不介入, 清空其积分, 避免切回平移时残留冲击 */
		g_heading_integral = 0.0f;
	}

	if (g_pid_enable && feedback_ok)
	{
		vx_cmd += pid_step(&g_pid_vx, g_target_vx, vx_meas, dt_s);
		vy_cmd += pid_step(&g_pid_vy, g_target_vy, vy_meas, dt_s);
		/* wz 反馈: 陀螺仪测真实角速度(能感知打滑导致的偏航), 否则退回编码器 */
		if (g_gyro_yaw_enable)
		{
			wz_cmd = wz_target + pid_step(&g_pid_wz, wz_target, gyro_wz, dt_s);
		}
		else
		{
			wz_cmd = wz_target + pid_step(&g_pid_wz, wz_target, wz_meas, dt_s);
		}
	}
	else
	{
		wz_cmd = wz_target;
	}

	/* 自转中心保持: 纯原地旋转时锁定世界坐标(起点在 set_target 锁存),
	   用陀螺航向把位置误差转到机体系后补偿平移, 消除"打着转飘移" */
	if (g_gyro_yaw_enable && feedback_ok && g_pure_rotate)
	{
		float ex = g_hold_x - g_pos_x;
		float ey = g_hold_y - g_pos_y;
		if (absf(ex) > CHASSIS_HOLD_POS_DEADZONE || absf(ey) > CHASSIS_HOLD_POS_DEADZONE)
		{
			float cos_t = cosf(g_heading);
			float sin_t = sinf(g_heading);
			vx_cmd += g_pos_hold_kp * ( ex * cos_t + ey * sin_t);
			vy_cmd += g_pos_hold_kp * (-ex * sin_t + ey * cos_t);
		}
	}

	vx_cmd = clampf(vx_cmd, -g_max_vx, g_max_vx);
	vy_cmd = clampf(vy_cmd, -g_max_vy, g_max_vy);
	wz_cmd = clampf(wz_cmd, -g_max_wz, g_max_wz);

	/* 平移态最终 wz 限幅: 航向保持的 wz_target 已限 0.05, 但 wz 环 PID 会再叠加
	   (out_limit 1.2), 使 wz_cmd 仍可达 ~2.0 rad/s、又吃掉后轮横移目标。
	   向左偏转更严重(质心偏移)时 wz 顶得更高, 故向左后轮仍不转; 这里平移时统一限幅。 */
	if (g_gyro_yaw_enable && absf(g_target_wz) < 0.01f)
	{
		wz_cmd = clampf(wz_cmd, -g_heading_wz_limit, g_heading_wz_limit);
	}

	body_to_wheel_omega(vx_cmd, vy_cmd, wz_cmd, wheel_omega);

	/* 单轮速度闭环(编码器): 每轮独立跟踪期望转速, 使四轮等速(直线时不走斜) */
	if (g_pid_enable && feedback_ok)
	{
		apply_wheel_speed_closed_loop(wheel_omega, dt_s);
		apply_wheel_speed_outlier_correction(wheel_omega);
	}

	max_cmd_rpm = 0.0f;
	for (i = 0; i < 4; i++)
	{
		float rpm = absf(wheel_omega[i] * 60.0f / (2.0f * CHASSIS_PI) * CHASSIS_GEAR_RATIO);
		if (rpm > max_cmd_rpm)
		{
			max_cmd_rpm = rpm;
		}
	}
	if (max_cmd_rpm > g_max_wheel_rpm && max_cmd_rpm > 1e-6f)
	{
		float scale = g_max_wheel_rpm / max_cmd_rpm;
		for (i = 0; i < 4; i++)
		{
			wheel_omega[i] *= scale;
		}
	}

	chassis_tx_schedule(wheel_omega);
}

void chassis_stop(void)
{
	chassis_set_target(0.0f, 0.0f, 0.0f);
	X_V2_Stop_Now(1, true);//
	chassis_update(0.002f);
}

/* ── 便捷运动控制 ────────────────────────────────── */
void chassis_forward(float speed_mps)
{
	chassis_set_target(speed_mps, 0.0f, 0.0f);
}

void chassis_backward(float speed_mps)
{
	chassis_set_target(-speed_mps, 0.0f, 0.0f);
}

void chassis_move_left(float speed_mps)
{
	chassis_set_target(0.0f, speed_mps, 0.0f);
}

void chassis_move_right(float speed_mps)
{
	chassis_set_target(0.0f, -speed_mps, 0.0f);
}

void chassis_turn(float radps)
{
	chassis_set_target(0.0f, 0.0f, radps);
}

void chassis_move_to(float x_m, float y_m, float theta_rad)
{
	g_target_pos_x = x_m;
	g_target_pos_y = y_m;
	g_target_pos_theta = theta_rad;
}

void chassis_position_reset(float x_m, float y_m, float theta_rad)
{
	g_pos_x = x_m;
	g_pos_y = y_m;
	g_pos_theta = theta_rad;
}

bool chassis_position_reached(float xy_tol_m, float theta_tol_rad)
{
	float ex = g_target_pos_x - g_pos_x;
	float ey = g_target_pos_y - g_pos_y;
	float et = wrap_pi(g_target_pos_theta - g_pos_theta);
	return ((ex * ex + ey * ey) <= (xy_tol_m * xy_tol_m)) && (absf(et) <= theta_tol_rad);
}

void chassis_get_position(float *x_m, float *y_m, float *theta_rad)
{
	*x_m      = g_pos_x;
	*y_m      = g_pos_y;
	*theta_rad = g_pos_theta;
}

void chassis_set_position_gains(float kp_xy, float kp_theta)
{
	g_pos_kp_xy    = absf(kp_xy);
	g_pos_kp_theta = absf(kp_theta);
}

/*
 * 位置控制更新函数——在位置模式下用此函数替代 chassis_update() 周期调用。
 *
 * 控制结构：
 *   外环  位置 P 控制器：世界系位置误差 → 机体系目标速度
 *   内环  速度 PID（复用 g_pid_vx/vy/wz）：目标速度 → 轮速指令
 *   里程计：用编码器反馈积分更新位姿估计（无反馈时冻结积分）
 *
 * 典型用法：
 *   chassis_position_reset(0, 0, 0);
 *   chassis_move_to(1.0f, 0.0f, 0.0f);
 *   while (!chassis_position_reached(0.03f, 0.05f))
 *       chassis_position_update(0.002f);
 */
void chassis_position_update(float dt_s)
{
	float vx_meas = 0.0f;
	float vy_meas = 0.0f;
	float wz_meas = 0.0f;
	float vx_cmd;
	float vy_cmd;
	float wz_cmd;
	float wheel_omega[4];
	bool  feedback_ok;
	float cos_t;
	float sin_t;
	float max_cmd_rpm;
	uint8_t i;

	if (!g_motor_enabled)
	{
		/* 电机失能时仅尝试重新使能（不阻塞），
		   完整初始化 (含 500ms 延时等 CAN 稳定) 已在 Chassis_Task 启动时完成 */
		uint8_t i;
		for (i = 0; i < 4; i++)
		{
			X_V2_En_Control(k_motor_ids[i], true, false);
		}
		g_motor_enabled = true;
	}

	/* 每拍请求四个电机速度回包(不再轮流), 保证位置模式下单轮速度闭环同样有实时反馈。 */
	X_V2_Read_Sys_Params(k_motor_ids[0], S_VEL);
	X_V2_Read_Sys_Params(k_motor_ids[1], S_VEL);
	X_V2_Read_Sys_Params(k_motor_ids[2], S_VEL);
	X_V2_Read_Sys_Params(k_motor_ids[3], S_VEL);

	feedback_ok = read_feedback_body_velocity(&vx_meas, &vy_meas, &wz_meas);

	/* 里程计：有反馈则积分测量速度，无反馈则冻结位姿。 */
	if (feedback_ok)
	{
		g_pos_theta = wrap_pi(g_pos_theta + wz_meas * dt_s);
		cos_t = cosf(g_pos_theta);
		sin_t = sinf(g_pos_theta);
		g_pos_x += (vx_meas * cos_t - vy_meas * sin_t) * dt_s;
		g_pos_y += (vx_meas * sin_t + vy_meas * cos_t) * dt_s;
	}

	/* 外环：位置 P 控制器，世界系误差变换到机体系后乘增益得目标速度。 */
	{
		float ex = g_target_pos_x - g_pos_x;
		float ey = g_target_pos_y - g_pos_y;
		float et = wrap_pi(g_target_pos_theta - g_pos_theta);

		cos_t = cosf(g_pos_theta);
		sin_t = sinf(g_pos_theta);

		vx_cmd = clampf(g_pos_kp_xy * ( ex * cos_t + ey * sin_t), -g_max_vx, g_max_vx);
		vy_cmd = clampf(g_pos_kp_xy * (-ex * sin_t + ey * cos_t), -g_max_vy, g_max_vy);
		wz_cmd = clampf(g_pos_kp_theta * et,                       -g_max_wz, g_max_wz);
	}

	/* 内环：速度 PID 修正（外环输出即为内环目标）。 */
	if (g_pid_enable && feedback_ok)
	{
		vx_cmd += pid_step(&g_pid_vx, vx_cmd, vx_meas, dt_s);
		vy_cmd += pid_step(&g_pid_vy, vy_cmd, vy_meas, dt_s);
		wz_cmd += pid_step(&g_pid_wz, wz_cmd, wz_meas, dt_s);
	}

	vx_cmd = clampf(vx_cmd, -g_max_vx, g_max_vx);
	vy_cmd = clampf(vy_cmd, -g_max_vy, g_max_vy);
	wz_cmd = clampf(wz_cmd, -g_max_wz, g_max_wz);

	body_to_wheel_omega(vx_cmd, vy_cmd, wz_cmd, wheel_omega);

	/* 单轮速度闭环(编码器): 每轮独立跟踪期望转速, 使四轮等速(直线时不走斜) */
	if (g_pid_enable && feedback_ok)
	{
		apply_wheel_speed_closed_loop(wheel_omega, dt_s);
		apply_wheel_speed_outlier_correction(wheel_omega);
	}

	max_cmd_rpm = 0.0f;
	for (i = 0; i < 4; i++)
	{
		float rpm = absf(wheel_omega[i] * 60.0f / (2.0f * CHASSIS_PI) * CHASSIS_GEAR_RATIO);
		if (rpm > max_cmd_rpm)
		{
			max_cmd_rpm = rpm;
		}
	}
	if (max_cmd_rpm > g_max_wheel_rpm && max_cmd_rpm > 1e-6f)
	{
		float scale = g_max_wheel_rpm / max_cmd_rpm;
		for (i = 0; i < 4; i++)
		{
			wheel_omega[i] *= scale;
		}
	}

	send_wheel_rpm_broadcast(wheel_omega);
}
