#ifndef __PID_H_
#define __PID_H_
 
#include "main.h"
#include "stm32f4xx.h"
 
typedef struct _pid_struct_t
{
  float kp;//比例
  float ki;//积分
  float kd;//微分
  float i_max;//积分限幅
  float out_max;//输出限幅
  
  float ref;      // target value目标角度
  float fdb;      // feedback value设定角度
  float err[2];   // error and last error差值
 
  float p_out;//比例输出
  float i_out;//积分输出
  float d_out;//微分输出
  float output;//pid总输出
}pid_struct_t;
 
void pid_init(pid_struct_t *pid,
              float kp,
              float ki,
              float kd,
              float i_max,
              float out_max);

void gimbal_PID_init(void);
float pid_calc(pid_struct_t *pid, float ref, float fdb);
void pid_reset(pid_struct_t *pid);

void    gimbal_goto_zero(void);
void    gimbal_goto_angle(float deg);    /* 通用: 转到任意角度(°) */
void    gimbal_goto_30deg(void);
void    gimbal_goto_60deg(void);
void    gimbal_goto_neg150deg(void);
void    gimbal_goto_neg150deg_reverse(void);

void    gimbal_step_to_zero(void);
void    gimbal_step_to_30deg(void);
void    gimbal_step_to_60deg(void);
void    gimbal_step_to_neg150deg(void);

int16_t gimbal_update(uint16_t raw_angle, int16_t raw_speed);
uint8_t gimbal_is_target_reached(uint16_t raw_angle, int16_t raw_speed);
 
#endif // __PID_H_