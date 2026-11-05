#include "ist8310driver.h"
#include "ist8310driver_middleware.h"

#define MAG_SEN 0.3f

#define IST8310_WHO_AM_I       0x00
#define IST8310_WHO_AM_I_VALUE 0x10

#define IST8310_WRITE_REG_NUM 4

static const uint8_t ist8310_write_reg_data_error[IST8310_WRITE_REG_NUM][3] = {
    {0x0B, 0x08, 0x01}, /* 使能中断，低电平极性 */
    {0x41, 0x09, 0x02}, /* 2次平均 */
    {0x42, 0xC0, 0x03}, /* 必须为 0xC0 */
    {0x0A, 0x0B, 0x04}, /* 200Hz 输出频率 */
};

uint8_t ist8310_whoami = 0;  /* 诊断：保存 WHO_AM_I 原始读值 */

uint8_t ist8310_init(void)
{
    static const uint8_t wait_time = 150;
    static const uint8_t sleepTime = 50;
    uint8_t res = 0;
    uint8_t writeNum = 0;

    ist8310_GPIO_init();
    ist8310_com_init();

    ist8310_RST_L();
    ist8310_delay_ms(sleepTime);
    ist8310_RST_H();
    ist8310_delay_ms(sleepTime);

    /* 尝试默认地址 0x0E */
    g_ist8310_i2c_addr = IST8310_IIC_ADDRESS;
    ist8310_whoami = ist8310_IIC_read_single_reg(IST8310_WHO_AM_I);
    if (ist8310_whoami != IST8310_WHO_AM_I_VALUE)
    {
        /* 尝试备用地址 0x0C (CAD0/1 均接 GND 时) */
        g_ist8310_i2c_addr = IST8310_IIC_ADDRESS_ALT;
        ist8310_whoami = ist8310_IIC_read_single_reg(IST8310_WHO_AM_I);
        if (ist8310_whoami != IST8310_WHO_AM_I_VALUE)
        {
            return IST8310_NO_SENSOR;
        }
    }

    for (writeNum = 0; writeNum < IST8310_WRITE_REG_NUM; writeNum++)
    {
        ist8310_IIC_write_single_reg(ist8310_write_reg_data_error[writeNum][0],
                                     ist8310_write_reg_data_error[writeNum][1]);
        ist8310_delay_us(wait_time);
        res = ist8310_IIC_read_single_reg(ist8310_write_reg_data_error[writeNum][0]);
        ist8310_delay_us(wait_time);
        if (res != ist8310_write_reg_data_error[writeNum][1])
        {
            return ist8310_write_reg_data_error[writeNum][2];
        }
    }
    return IST8310_NO_ERROR;
}

void ist8310_read_over(uint8_t *status_buf, ist8310_real_data_t *ist8310_real_data)
{
    if (status_buf[0] & 0x01)
    {
        int16_t temp_ist8310_data = 0;
        ist8310_real_data->status |= 1 << IST8310_DATA_READY_BIT;

        temp_ist8310_data = (int16_t)((status_buf[2] << 8) | status_buf[1]);
        ist8310_real_data->mag[0] = MAG_SEN * temp_ist8310_data;
        temp_ist8310_data = (int16_t)((status_buf[4] << 8) | status_buf[3]);
        ist8310_real_data->mag[1] = MAG_SEN * temp_ist8310_data;
        temp_ist8310_data = (int16_t)((status_buf[6] << 8) | status_buf[5]);
        ist8310_real_data->mag[2] = MAG_SEN * temp_ist8310_data;
    }
    else
    {
        ist8310_real_data->status &= ~(1 << IST8310_DATA_READY_BIT);
    }
}

void ist8310_read_mag(fp32 mag[3])
{
    uint8_t buf[6];
    int16_t temp_ist8310_data = 0;
    /* 从 DATAXL 寄存器 (0x03) 开始连续读取 6 字节 */
    ist8310_IIC_read_muli_reg(0x03, buf, 6);

    temp_ist8310_data = (int16_t)((buf[1] << 8) | buf[0]);
    mag[0] = MAG_SEN * temp_ist8310_data;
    temp_ist8310_data = (int16_t)((buf[3] << 8) | buf[2]);
    mag[1] = MAG_SEN * temp_ist8310_data;
    temp_ist8310_data = (int16_t)((buf[5] << 8) | buf[4]);
    mag[2] = MAG_SEN * temp_ist8310_data;
}
