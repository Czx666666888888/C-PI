#ifndef TEST_USART_H
#define TEST_USART_H
#include <stdint.h>
typedef struct {int unused;} UART_HandleTypeDef;
#define HAL_OK 0
uint32_t HAL_GetTick(void);
int HAL_UART_Transmit(UART_HandleTypeDef *,uint8_t *,uint16_t,uint32_t);
extern UART_HandleTypeDef huart3;
#endif
