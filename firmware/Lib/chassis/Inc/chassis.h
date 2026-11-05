#ifndef __CHASSIS_H__
#define __CHASSIS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
	float kp;
	float ki;
	float kd;
	float i_limit;
	float out_limit;
} chassis_pid_gain_t;

/* 速度控制接口（原有） */
void chassis_init(void);
void chassis_gyro_calibrate(void);   /* 陀螺仪零偏标定(需在车辆静止时调用一次) */
void chassis_set_gyro_yaw_enable(bool enable);  /* 使能/关闭陀螺仪偏航闭环 */
void chassis_set_heading_hold_gain(float kp);    /* 航向保持 P 增益 (rad/s per rad) */
void chassis_set_rotation_hold_gain(float kp);   /* 自转中心保持 P 增益 (m/s per m) */
void chassis_set_target(float vx_mps, float vy_mps, float wz_radps);
void chassis_set_max_motion(float max_vx_mps, float max_vy_mps, float max_wz_radps, float max_wheel_rpm);
void chassis_set_pid_enable(bool enable);
void chassis_set_pid_gains(const chassis_pid_gain_t *vx_gain, const chassis_pid_gain_t *vy_gain, const chassis_pid_gain_t *wz_gain);
void chassis_set_wheel_pid_gains(const chassis_pid_gain_t *gain);   /* 单轮速度闭环 PID 增益(四个轮共用) */
void chassis_get_wheel_rpm(float wheel_rpm[4]);   /* 读取四轮实测转速(RPM, 底盘坐标系带符号, 轮序: 前左/前右/后左/后右) */
void chassis_get_wheel_cmd_rpm(float wheel_rpm[4]); /* 读取最近下发的四轮目标转速(同坐标系, 诊断用) */
void chassis_update(float dt_s);
void chassis_stop(void);

/* 便捷运动控制 */
void chassis_forward( float speed_mps);   /* 前进 */
void chassis_backward(float speed_mps);   /* 后退 */
void chassis_move_left(float speed_mps);  /* 左移 */
void chassis_move_right(float speed_mps); /* 右移 */
void chassis_turn(float radps);           /* 原地旋转 */

/* 位置控制接口 */
void chassis_move_to(float x_m, float y_m, float theta_rad);
void chassis_position_reset(float x_m, float y_m, float theta_rad);
bool chassis_position_reached(float xy_tol_m, float theta_tol_rad);
void chassis_get_position(float *x_m, float *y_m, float *theta_rad);
void chassis_set_position_gains(float kp_xy, float kp_theta);
void chassis_position_update(float dt_s);

#ifdef __cplusplus
}
#endif

#endif
