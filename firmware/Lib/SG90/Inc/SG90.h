#ifndef SG90_H
#define SG90_H

#include "main.h"
#include "tim.h"
#include "stm32f4xx_hal_tim.h"
#include <stdint.h>
// Function prototypes
void SG90_Init(void);
void SG90_SetAngle(int16_t angle);

#endif // SG90_H