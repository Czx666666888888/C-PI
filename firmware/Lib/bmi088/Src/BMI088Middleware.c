#include "BMI088Middleware.h"
#include "main.h"
#include "stm32f4xx_hal.h"

extern SPI_HandleTypeDef hspi1;

/* ── DWT 初始化 (Cortex-M4 调试功能, 用于微秒级延时) ── */
static void BMI088_DWT_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void BMI088_GPIO_init(void)
{
    /* CS 引脚已在 MX_GPIO_Init() 中初始化 (PB2=ACCEL_CS, PB0=GYRO_CS) */
}

void BMI088_com_init(void)
{
    /* SPI1 已在 MX_SPI1_Init() 中初始化, 此处仅初始化 DWT */
    BMI088_DWT_Init();
}

void BMI088_delay_ms(uint16_t ms)
{
    while(ms--)
    {
        BMI088_delay_us(1000);
    }
}

void BMI088_delay_us(uint16_t us)
{
    uint32_t ticks = us * (SystemCoreClock / 1000000U);
    uint32_t start  = DWT->CYCCNT;
    while ((DWT->CYCCNT - start) < ticks);
}

void BMI088_ACCEL_NS_L(void)
{
    HAL_GPIO_WritePin(BMI088_ACCEL_CS_GPIO_Port, BMI088_ACCEL_CS_Pin, GPIO_PIN_RESET);
}
void BMI088_ACCEL_NS_H(void)
{
    HAL_GPIO_WritePin(BMI088_ACCEL_CS_GPIO_Port, BMI088_ACCEL_CS_Pin, GPIO_PIN_SET);
}

void BMI088_GYRO_NS_L(void)
{
    HAL_GPIO_WritePin(BMI088_GYRO_CS_GPIO_Port, BMI088_GYRO_CS_Pin, GPIO_PIN_RESET);
}
void BMI088_GYRO_NS_H(void)
{
    HAL_GPIO_WritePin(BMI088_GYRO_CS_GPIO_Port, BMI088_GYRO_CS_Pin, GPIO_PIN_SET);
}

uint8_t BMI088_read_write_byte(uint8_t txdata)
{
    uint8_t rx_data;
    HAL_SPI_TransmitReceive(&hspi1, &txdata, &rx_data, 1, 1000);
    return rx_data;
}
