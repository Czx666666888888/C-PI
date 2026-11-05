#ifndef __LIFT_H__
#define __LIFT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
	float screw_pitch_mm;
	float min_height_mm;
	float max_height_mm;
	float max_vel_rpm;
	float acc_rpmps;
	float dec_rpmps;
	int8_t cmd_dir_sign;
} lift_config_t;

void lift_init(void);
void lift_init_with_config(const lift_config_t *config);
void lift_enable(bool enable);
void lift_set_zero_height(float current_height_mm);
void lift_define_current_as_zero(void);
void lift_set_height_limit(float min_height_mm, float max_height_mm);
void lift_set_motion_limit(float max_vel_rpm, float acc_rpmps, float dec_rpmps);
void lift_set_target_height(float target_height_mm);
void lift_move_up_mm(float delta_mm);
void lift_move_down_mm(float delta_mm);
void lift_force_move_up_mm(float delta_mm);
void lift_start_down_up_test(float down_mm, float up_mm);
bool lift_is_test_done(void);
/* 可选：仅用于周期拉取反馈和运行测试状态机，基础升降不再依赖此函数。 */
void lift_update(float dt_s);
void lift_stop(void);

bool lift_is_target_reached(float tol_mm);
bool lift_has_feedback(void);
float lift_get_height_mm(void);
float lift_get_target_height_mm(void);
float lift_get_speed_rpm(void);

void lift_can_parse_xv2_feedback(uint32_t ext_id, const uint8_t *rx_data, uint8_t dlc);

#ifdef __cplusplus
}
#endif

#endif
