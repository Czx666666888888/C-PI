#ifndef USB_CPI_H
#define USB_CPI_H
#include "main.h"
void USB_CPI_Init(void);
int USB_CPI_Send(const uint8_t *data, uint16_t length);
#endif
