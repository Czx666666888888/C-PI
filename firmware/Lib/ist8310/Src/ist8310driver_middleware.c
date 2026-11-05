#include "ist8310driver_middleware.h"
#include "main.h"
#include "gpio.h"

extern I2C_HandleTypeDef hi2c3;

uint8_t g_ist8310_i2c_addr = IST8310_IIC_ADDRESS;  /* 默认 0x0E, init 可能改为 0x0C */

uint8_t ist8310_i2c_diag   = 0;  /* 诊断: 最后一次 I2C 操作的 HAL 返回值 */
uint32_t ist8310_i2c_sr1   = 0;  /* 诊断: I2C3 SR1 错误标志寄存器 */

/* GPIO 已在 MX_GPIO_Init 中初始化 (RST=PG6, DRDY=PG3) */
void ist8310_GPIO_init(void)
{
    /* RST 和 DRDY 引脚已在 gpio.c MX_GPIO_Init() 中配置 */
}

/* I2C3 已在 MX_I2C3_Init 中初始化 (SCL=PA8, SDA=PC9, 100kHz) */
void ist8310_com_init(void)
{
    /* 通信接口已在 i2c.c MX_I2C3_Init() 中配置 */

    /* 使能 DWT Cycle Counter 用于微秒延时（SysTick 由 TIM2 替代，FreeRTOS 启动前不可用）*/
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT      = 0;
    DWT->CTRL        |= DWT_CTRL_CYCCNTENA_Msk;
}

uint8_t ist8310_IIC_read_single_reg(uint8_t reg)
{
    uint8_t res = 0;
    HAL_StatusTypeDef status;
    status = HAL_I2C_Mem_Read(&hi2c3, (uint16_t)(g_ist8310_i2c_addr << 1),
                              reg, I2C_MEMADD_SIZE_8BIT, &res, 1, 10);
    ist8310_i2c_diag = (uint8_t)status;
    if (status != HAL_OK) {
        ist8310_i2c_sr1 = hi2c3.Instance->SR1;  /* 保存错误现场 */
    }
    return res;
}

void ist8310_IIC_write_single_reg(uint8_t reg, uint8_t data)
{
    HAL_StatusTypeDef status;
    status = HAL_I2C_Mem_Write(&hi2c3, (uint16_t)(g_ist8310_i2c_addr << 1),
                               reg, I2C_MEMADD_SIZE_8BIT, &data, 1, 10);
    ist8310_i2c_diag = (uint8_t)status;
    if (status != HAL_OK) {
        ist8310_i2c_sr1 = hi2c3.Instance->SR1;
    }
}

void ist8310_IIC_read_muli_reg(uint8_t reg, uint8_t *buf, uint8_t len)
{
    HAL_StatusTypeDef status;
    status = HAL_I2C_Mem_Read(&hi2c3, (uint16_t)(g_ist8310_i2c_addr << 1),
                              reg, I2C_MEMADD_SIZE_8BIT, buf, len, 10);
    ist8310_i2c_diag = (uint8_t)status;
    if (status != HAL_OK) {
        ist8310_i2c_sr1 = hi2c3.Instance->SR1;
    }
}

void ist8310_IIC_write_muli_reg(uint8_t reg, uint8_t *data, uint8_t len)
{
    HAL_StatusTypeDef status;
    status = HAL_I2C_Mem_Write(&hi2c3, (uint16_t)(g_ist8310_i2c_addr << 1),
                               reg, I2C_MEMADD_SIZE_8BIT, data, len, 10);
    ist8310_i2c_diag = (uint8_t)status;
    if (status != HAL_OK) {
        ist8310_i2c_sr1 = hi2c3.Instance->SR1;
    }
}

void ist8310_delay_ms(uint16_t ms)
{
    HAL_Delay(ms);
}

void ist8310_delay_us(uint16_t us)
{
    /* DWT Cycle Counter 延时，不依赖 SysTick，FreeRTOS 启动前后均可使用 */
    uint32_t start = DWT->CYCCNT;
    uint32_t target = us * (SystemCoreClock / 1000000UL);
    while ((DWT->CYCCNT - start) < target);
}

void ist8310_RST_H(void)
{
    HAL_GPIO_WritePin(IST8310_RST_GPIO_Port, IST8310_RST_Pin, GPIO_PIN_SET);
}

void ist8310_RST_L(void)
{
    HAL_GPIO_WritePin(IST8310_RST_GPIO_Port, IST8310_RST_Pin, GPIO_PIN_RESET);
}
