#include "6020_pid.h"
 
pid_struct_t gimbal_yaw_speed_pid;
pid_struct_t gimbal_yaw_angle_pid;

#ifndef LIMIT_MIN_MAX
#define LIMIT_MIN_MAX(val, min, max) \
  do {                                \
    if ((val) < (min))                \
      (val) = (min);                  \
    else if ((val) > (max))           \
      (val) = (max);                  \
  } while (0)
#endif
 
void pid_init(pid_struct_t *pid,
              float kp,
              float ki,
              float kd,
              float i_max,
              float out_max)//PID初始化函数
{
  pid->kp      = kp;
  pid->ki      = ki;
  pid->kd      = kd;
  pid->i_max   = i_max;
  pid->out_max = out_max;
}

void pid_reset(pid_struct_t *pid)
{
  pid->ref    = 0.0f;
  pid->fdb    = 0.0f;
  pid->err[0] = 0.0f;
  pid->err[1] = 0.0f;
  pid->p_out  = 0.0f;
  pid->i_out  = 0.0f;
  pid->d_out  = 0.0f;
  pid->output = 0.0f;
}

float pid_calc(pid_struct_t *pid, float ref, float fdb)//PID运算函数
{
  pid->ref = ref;
  pid->fdb = fdb;
  pid->err[1] = pid->err[0];
  pid->err[0] = pid->ref - pid->fdb;
  
  pid->p_out  = pid->kp * pid->err[0];
  pid->i_out += pid->ki * pid->err[0];
  pid->d_out  = pid->kd * (pid->err[0] - pid->err[1]);
  LIMIT_MIN_MAX(pid->i_out, -pid->i_max, pid->i_max);
  
  pid->output = pid->p_out + pid->i_out + pid->d_out;
  LIMIT_MIN_MAX(pid->output, -pid->out_max, pid->out_max);
  return pid->output;
}
 
/* 编码器 1 圈 = 8192 counts，1° = 8192/360 ≈ 22.756 counts */
#define DEG_TO_CNT(deg)  ((deg) * 8192.0f / 360.0f)
#define DEADBAND_CNT     15.0f
#define SPEED_DEADBAND   80
#define DIR_SWITCH_CNT   300.0f

typedef enum
{
  GIMBAL_DIR_AUTO = 0,
  GIMBAL_DIR_FORCE_POS,
  GIMBAL_DIR_FORCE_NEG
} gimbal_dir_mode_t;

static float gimbal_target_cnt = 0.0f; /* 当前目标，单位：有符号 encoder count */
static gimbal_dir_mode_t gimbal_dir_mode = GIMBAL_DIR_AUTO;

static float wrap_encoder_err(float err)
{
  while (err > 4096.0f)  err -= 8192.0f;
  while (err < -4096.0f) err += 8192.0f;
  return err;
}

static float calc_directed_err(float target_cnt, float fdb_cnt)
{
  float raw_err = target_cnt - fdb_cnt;
  float short_err = wrap_encoder_err(raw_err);
  float abs_short_err = (short_err >= 0.0f) ? short_err : -short_err;

  /* Near target, always use shortest-path error to brake and settle. */
  if (abs_short_err <= DIR_SWITCH_CNT)
  {
    return short_err;
  }

  if (gimbal_dir_mode == GIMBAL_DIR_FORCE_POS)
  {
    while (raw_err <= 0.0f) raw_err += 8192.0f;
    while (raw_err > 8192.0f) raw_err -= 8192.0f;
    return raw_err;
  }

  if (gimbal_dir_mode == GIMBAL_DIR_FORCE_NEG)
  {
    while (raw_err >= 0.0f) raw_err -= 8192.0f;
    while (raw_err < -8192.0f) raw_err += 8192.0f;
    return raw_err;
  }

  return short_err;
}

void gimbal_PID_init(void)
{
  /* 塔吊旋转 6020 速度环: 纯 P(ki=0) 跟随摇杆, 回中即停; kp 决定力矩/灵敏度, out_max 是电压上限 */
  pid_init(&gimbal_yaw_speed_pid, 50.0f, 0.0f, 0.2f, 0.0f, 30000.0f);
  pid_init(&gimbal_yaw_angle_pid, 8.0f, 0.0f, 0.5f, 0.0f, 400.0f);
    gimbal_target_cnt = 0.0f;
    gimbal_dir_mode = GIMBAL_DIR_AUTO;
}

/* ---------- 目标设定函数 ---------- */
void gimbal_goto_zero(void)      { gimbal_target_cnt = DEG_TO_CNT(  0.0f); gimbal_dir_mode = GIMBAL_DIR_AUTO; }
void gimbal_goto_angle(float deg){ gimbal_target_cnt = DEG_TO_CNT(   deg); gimbal_dir_mode = GIMBAL_DIR_AUTO; }
void gimbal_goto_30deg(void)     { gimbal_target_cnt = DEG_TO_CNT( 30.0f); gimbal_dir_mode = GIMBAL_DIR_AUTO; }
void gimbal_goto_60deg(void)     { gimbal_target_cnt = DEG_TO_CNT( 60.0f); gimbal_dir_mode = GIMBAL_DIR_AUTO; }
void gimbal_goto_neg150deg(void) { gimbal_target_cnt = DEG_TO_CNT(-150.0f); gimbal_dir_mode = GIMBAL_DIR_AUTO; }
void gimbal_goto_neg150deg_reverse(void)
{
  gimbal_target_cnt = DEG_TO_CNT(-150.0f);
  gimbal_dir_mode = GIMBAL_DIR_FORCE_NEG;
}

void gimbal_step_to_zero(void)
{
  gimbal_goto_zero();
}

void gimbal_step_to_30deg(void)
{
  gimbal_goto_30deg();
}

void gimbal_step_to_60deg(void)
{
  gimbal_goto_60deg();
}

void gimbal_step_to_neg150deg(void)
{
  gimbal_goto_neg150deg_reverse();
}

/* ---------- 主循环调用：输入当前角度和转速，返回电压指令 ---------- */
int16_t gimbal_update(uint16_t raw_angle, int16_t raw_speed)
{
  float angle_fdb = (float)raw_angle;
  if (angle_fdb > 4095.0f) angle_fdb -= 8192.0f;

    float err = calc_directed_err(gimbal_target_cnt, angle_fdb);

    if (err > DEADBAND_CNT || err < -DEADBAND_CNT)
    {
        float speed_ref = pid_calc(&gimbal_yaw_angle_pid, 0.0f, -err);
        float voltage   = pid_calc(&gimbal_yaw_speed_pid, speed_ref, (float)raw_speed);
        return (int16_t)voltage;
    }
    return 0;
}

uint8_t gimbal_is_target_reached(uint16_t raw_angle, int16_t raw_speed)
{
    float angle_fdb = (float)raw_angle;
    if (angle_fdb > 4095.0f) angle_fdb -= 8192.0f;
    float err = wrap_encoder_err(gimbal_target_cnt - angle_fdb);
    if (err < 0.0f) err = -err;

    int16_t speed_abs = (raw_speed >= 0) ? raw_speed : (int16_t)(-raw_speed);
    if (err <= DEADBAND_CNT && speed_abs <= SPEED_DEADBAND)
    {
      return 1U;
    }
    return 0U;
}