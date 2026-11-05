#include "bsp_adc.h"
#include "main.h"

extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc3;

/* 内部参考电压校准系数 — 上电后由 init_vrefint_reciprocal() 计算 */
volatile float voltage_vrefint_proportion = 8.0586080586080586080586080586081e-4f;

/**
  * @brief  读取 ADC 指定通道的单次采样值
  * @param  ADCx: ADC 句柄指针
  * @param  ch: ADC 通道号
  * @retval 12-bit ADC 原始值
  */
static uint16_t adcx_get_chx_value(ADC_HandleTypeDef *ADCx, uint32_t ch)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel      = ch;
    sConfig.Rank         = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;

    if (HAL_ADC_ConfigChannel(ADCx, &sConfig) != HAL_OK)
    {
        Error_Handler();
    }

    HAL_ADC_Start(ADCx);
    if (HAL_ADC_PollForConversion(ADCx, 10) != HAL_OK)
    {
        return 0;  /* 超时/错误时返回 0，由调用方判零跳过 */
    }
    return (uint16_t)HAL_ADC_GetValue(ADCx);
}

/**
  * @brief  利用内部 VREFINT (1.2V) 校准 ADC 比例系数
  * @note   取 200 次平均值，提高校准精度
  * @retval 无
  */
void init_vrefint_reciprocal(void)
{
    uint8_t i;
    uint16_t adc_val;
    uint32_t total_adc = 0U;
    uint8_t retry;

    for (i = 0; i < 200; i++)
    {
        /* 单次 ADC 读取 + 超时重试（最多 3 次）*/
        for (retry = 0; retry < 3; retry++)
        {
            adc_val = adcx_get_chx_value(&hadc1, ADC_CHANNEL_VREFINT);
            if (adc_val > 0) break;
            HAL_Delay(1);
        }
        total_adc += adc_val;
    }

    if (total_adc > 0)
    {
        voltage_vrefint_proportion = (float)(200U) * 1.2f / (float)total_adc;
    }
    /* else: 保留默认值，避免除零 */
}

/**
  * @brief  获取芯片内部温度
  * @retval 温度值 (°C)
  */
float get_temperature(void)
{
    float temperate;
    uint16_t adcx = adcx_get_chx_value(&hadc1, ADC_CHANNEL_TEMPSENSOR);
    temperate = (float)adcx * voltage_vrefint_proportion;
    temperate = (temperate - 0.76f) * 400.0f + 25.0f;
    return temperate;
}

/**
  * @brief  获取电池电压 (通过 ADC3_IN8, PF10)
  * @note   电压分压比: (22K + 200K) / 22K ≈ 10.0909
  * @retval 电池电压值 (V)
  */
float get_battery_voltage(void)
{
    float voltage;
    uint16_t adcx = adcx_get_chx_value(&hadc3, ADC_CHANNEL_8);
    voltage = (float)adcx * voltage_vrefint_proportion * 10.090909090909090909090909090909f;
    return voltage;
}
