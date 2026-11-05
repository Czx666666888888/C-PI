#ifndef IST8310DRIVER_MIDDLEWARE_H
#define IST8310DRIVER_MIDDLEWARE_H

#include "struct_typedef.h"

#define IST8310_IIC_ADDRESS 0x0E
#define IST8310_IIC_ADDRESS_ALT 0x0C  /* CAD0/1 均接 GND 时的地址 */

extern uint8_t g_ist8310_i2c_addr;    /* 当前使用的 I2C 地址, init 中自动探测 */
extern uint8_t ist8310_i2c_diag;      /* 诊断: HAL 返回值 (0=OK,1=ERROR,2=BUSY,3=TIMEOUT) */
extern uint32_t ist8310_i2c_sr1;      /* 诊断: I2C3 SR1 寄存器 (错误标志位) */

extern void ist8310_GPIO_init(void);
extern void ist8310_com_init(void);
extern uint8_t ist8310_IIC_read_single_reg(uint8_t reg);
extern void ist8310_IIC_write_single_reg(uint8_t reg, uint8_t data);
extern void ist8310_IIC_read_muli_reg(uint8_t reg, uint8_t *buf, uint8_t len);
extern void ist8310_IIC_write_muli_reg(uint8_t reg, uint8_t *data, uint8_t len);
extern void ist8310_delay_ms(uint16_t ms);
extern void ist8310_delay_us(uint16_t us);
extern void ist8310_RST_H(void);
extern void ist8310_RST_L(void);

#endif
