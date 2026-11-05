#include "SG90.h"
#include "tim.h"
#include "stm32f4xx_hal_tim.h"
#include "gpio.h"

void SG90_Init(void)
{
    /* TIM1 CH1 on PE9: configured at 50Hz PWM in CubeMX.
       PSC=167, Period=19999 → 168MHz/(168*20000)=50Hz
       Pulse range: 1000(1ms=-90°) ~ 1500(1.5ms=0°) ~ 2000(2ms=+90°) */
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 1500);
}

void SG90_SetAngle(int16_t angle)
{
    if (angle > 90) angle = 90;
    if (angle < -90) angle = -90;
    uint16_t pulse = (uint16_t)(1500 + ((int32_t)angle * 1000) / 90);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, pulse);
}
