#include "main.h"
#include "gpio.h"
#include "OLED_Font.h"

/* 控制器配置：2.42寸I2C屏适配 SSD1309 */
#define OLED_DRIVER_SH1106 1
#define OLED_DRIVER_SSD1306 2
#define OLED_DRIVER_SSD1309 3

#ifndef OLED_DRIVER_CHIP
#define OLED_DRIVER_CHIP OLED_DRIVER_SSD1309
#endif

/* SH1106常有2列偏移，SSD1306/SSD1309一般为0 */
#if (OLED_DRIVER_CHIP == OLED_DRIVER_SH1106)
#define OLED_COLUMN_OFFSET 2
#else
#define OLED_COLUMN_OFFSET 0
#endif

/* 引脚配置: PF1=SCL, PF0=SDA */
#define OLED_W_SCL(x)    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_1, (GPIO_PinState)(x))
#define OLED_W_SDA(x)    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_0, (GPIO_PinState)(x))

/* 软件 I2C 时序延时：降低边沿速度，提升总线稳定性 */
static void OLED_I2C_Delay(void)
{
    volatile uint8_t i;
    for (i = 0; i < 12; i++)
    {
        __NOP();
    }
}

/* 引脚初始化：将 PF0/PF1 强制切为 GPIO 开漏输出（覆盖 I2C2 AF 配置） */
void OLED_I2C_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* GPIOF 时钟已在 main.c 中通过 MX_GPIO_Init 使能 */

    /* 先拉高再配置，避免总线上产生毛刺 */
    HAL_GPIO_WritePin(GPIOF, GPIO_PIN_0 | GPIO_PIN_1, GPIO_PIN_SET);

    GPIO_InitStruct.Pin   = GPIO_PIN_0 | GPIO_PIN_1;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;             /* 外部上拉 */
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    OLED_W_SCL(1);
    OLED_W_SDA(1);
}

/**
  * @brief  I2C开始
  * @param  无
  * @retval 无
  */
void OLED_I2C_Start(void)
{
    OLED_W_SDA(1);
    OLED_W_SCL(1);
    OLED_I2C_Delay();
    OLED_W_SDA(0);
    OLED_I2C_Delay();
    OLED_W_SCL(0);
    OLED_I2C_Delay();
}

/**
  * @brief  I2C停止
  * @param  无
  * @retval 无
  */
void OLED_I2C_Stop(void)
{
    OLED_W_SDA(0);
    OLED_I2C_Delay();
    OLED_W_SCL(1);
    OLED_I2C_Delay();
    OLED_W_SDA(1);
    OLED_I2C_Delay();
}

/**
  * @brief  I2C发送一个字节（不处理应答）
  * @param  Byte 要发送的一个字节
  * @retval 无
  */
void OLED_I2C_SendByte(uint8_t Byte)
{
    uint8_t i;
    for (i = 0; i < 8; i++)
    {
        OLED_W_SDA(!!(Byte & (0x80 >> i)));
        OLED_I2C_Delay();
        OLED_W_SCL(1);
        OLED_I2C_Delay();
        OLED_W_SCL(0);
        OLED_I2C_Delay();
    }
    OLED_W_SCL(1);    /* 额外的一个时钟，不处理应答信号 */
    OLED_I2C_Delay();
    OLED_W_SCL(0);
    OLED_I2C_Delay();
}

/**
  * @brief  OLED写命令
  * @param  Command 要写入的命令
  * @retval 无
  */
void OLED_WriteCommand(uint8_t Command)
{
    OLED_I2C_Start();
    OLED_I2C_SendByte(0x78);    /* 从机地址 */
    OLED_I2C_SendByte(0x00);    /* 写命令 */
    OLED_I2C_SendByte(Command);
    OLED_I2C_Stop();
}

/**
  * @brief  OLED写数据
  * @param  Data 要写入的数据
  * @retval 无
  */
void OLED_WriteData(uint8_t Data)
{
    OLED_I2C_Start();
    OLED_I2C_SendByte(0x78);    /* 从机地址 */
    OLED_I2C_SendByte(0x40);    /* 写数据 */
    OLED_I2C_SendByte(Data);
    OLED_I2C_Stop();
}

/**
  * @brief  将1列8像素数据按纵向2倍放大为16像素，并拆分成两个page字节
  * @param  Source 原始8位列数据（bit0为最上方像素）
  * @param  PageLow 放大后低8位page数据
  * @param  PageHigh 放大后高8位page数据
  * @retval 无
  */
static void OLED_ScaleColumn8To16(uint8_t Source, uint8_t *PageLow, uint8_t *PageHigh)
{
    uint8_t bit;
    uint16_t expanded = 0;

    for (bit = 0; bit < 8; bit++)
    {
        if (Source & (1U << bit))
        {
            expanded |= (uint16_t)(3U << (bit * 2));
        }
    }

    *PageLow = (uint8_t)(expanded & 0x00FFU);
    *PageHigh = (uint8_t)((expanded >> 8) & 0x00FFU);
}

/**
  * @brief  OLED设置光标位置
  * @param  Y 以左上角为原点，向下方向的坐标，范围：0~7
  * @param  X 以左上角为原点，向右方向的坐标，范围：0~127
  * @retval 无
  */
void OLED_SetCursor(uint8_t Y, uint8_t X)
{
    X += OLED_COLUMN_OFFSET;
    OLED_WriteCommand(0xB0 | Y);                    /* 设置Y位置 */
    OLED_WriteCommand(0x10 | ((X & 0xF0) >> 4));    /* 设置X位置高4位 */
    OLED_WriteCommand(0x00 | (X & 0x0F));            /* 设置X位置低4位 */
}

/**
  * @brief  OLED清屏
  * @param  无
  * @retval 无
  */
void OLED_Clear(void)
{
    uint8_t i, j;
    for (j = 0; j < 8; j++)
    {
        OLED_SetCursor(j, 0);
        for (i = 0; i < 128; i++)
        {
            OLED_WriteData(0x00);
        }
    }
}

/**
  * @brief  OLED显示一个字符
  * @param  Line 行位置（大字），范围：1~2
  * @param  Column 列位置（大字），范围：1~8
  * @param  Char 要显示的一个字符，范围：ASCII可见字符
  * @retval 无
  */
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char)
{
    uint8_t i;
    uint8_t page0, page1;
    uint8_t xStart = (Column - 1) * 16;
    uint8_t yStart = (Line - 1) * 4;

    if (Line < 1 || Line > 2 || Column < 1 || Column > 8)
    {
        return;
    }
    if (Char < ' ' || Char > '~')
    {
        Char = '?';
    }

    OLED_SetCursor(yStart, xStart);
    for (i = 0; i < 8; i++)
    {
        OLED_ScaleColumn8To16(OLED_F8x16[Char - ' '][i], &page0, &page1);
        OLED_WriteData(page0);
        OLED_WriteData(page0);
    }

    OLED_SetCursor(yStart + 1, xStart);
    for (i = 0; i < 8; i++)
    {
        OLED_ScaleColumn8To16(OLED_F8x16[Char - ' '][i], &page0, &page1);
        OLED_WriteData(page1);
        OLED_WriteData(page1);
    }

    OLED_SetCursor(yStart + 2, xStart);
    for (i = 0; i < 8; i++)
    {
        OLED_ScaleColumn8To16(OLED_F8x16[Char - ' '][i + 8], &page0, &page1);
        OLED_WriteData(page0);
        OLED_WriteData(page0);
    }

    OLED_SetCursor(yStart + 3, xStart);
    for (i = 0; i < 8; i++)
    {
        OLED_ScaleColumn8To16(OLED_F8x16[Char - ' '][i + 8], &page0, &page1);
        OLED_WriteData(page1);
        OLED_WriteData(page1);
    }
}

/**
  * @brief  OLED显示字符串
  * @param  Line 起始行位置（大字），范围：1~2
  * @param  Column 起始列位置（大字），范围：1~8
  * @param  String 要显示的字符串，范围：ASCII可见字符
  * @retval 无
  */
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String)
{
    uint8_t i;
    for (i = 0; String[i] != '\0' && (Column + i) <= 8; i++)
    {
        OLED_ShowChar(Line, Column + i, String[i]);
    }
}

/**
  * @brief  OLED次方函数
  * @retval 返回值等于X的Y次方
  */
uint32_t OLED_Pow(uint32_t X, uint32_t Y)
{
    uint32_t Result = 1;
    while (Y--)
    {
        Result *= X;
    }
    return Result;
}

/**
  * @brief  OLED显示数字（十进制，正数）
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：0~4294967295
  * @param  Length 要显示数字的长度，范围：1~10
  * @retval 无
  */
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
    uint8_t i;
    for (i = 0; i < Length; i++)
    {
        OLED_ShowChar(Line, Column + i, Number / OLED_Pow(10, Length - i - 1) % 10 + '0');
    }
}

/**
  * @brief  OLED显示数字（十进制，带符号数）
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：-2147483648~2147483647
  * @param  Length 要显示数字的长度，范围：1~10
  * @retval 无
  */
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length)
{
    uint8_t i;
    uint32_t Number1;
    if (Number >= 0)
    {
        OLED_ShowChar(Line, Column, '+');
        Number1 = Number;
    }
    else
    {
        OLED_ShowChar(Line, Column, '-');
        Number1 = -Number;
    }
    for (i = 0; i < Length; i++)
    {
        OLED_ShowChar(Line, Column + i + 1, Number1 / OLED_Pow(10, Length - i - 1) % 10 + '0');
    }
}

/**
  * @brief  OLED显示数字（十六进制，正数）
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：0~0xFFFFFFFF
  * @param  Length 要显示数字的长度，范围：1~8
  * @retval 无
  */
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
    uint8_t i, SingleNumber;
    for (i = 0; i < Length; i++)
    {
        SingleNumber = Number / OLED_Pow(16, Length - i - 1) % 16;
        if (SingleNumber < 10)
        {
            OLED_ShowChar(Line, Column + i, SingleNumber + '0');
        }
        else
        {
            OLED_ShowChar(Line, Column + i, SingleNumber - 10 + 'A');
        }
    }
}

/**
  * @brief  OLED显示数字（二进制，正数）
  * @param  Line 起始行位置，范围：1~4
  * @param  Column 起始列位置，范围：1~16
  * @param  Number 要显示的数字，范围：0~1111 1111 1111 1111
  * @param  Length 要显示数字的长度，范围：1~16
  * @retval 无
  */
void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
    uint8_t i;
    for (i = 0; i < Length; i++)
    {
        OLED_ShowChar(Line, Column + i, Number / OLED_Pow(2, Length - i - 1) % 2 + '0');
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * 6x8 小字体 显示函数
 * 适用范围：Line 1~8（物理 page 0~7），Column 1~21（6px×21=126px）
 * ═══════════════════════════════════════════════════════════════════ */

/**
  * @brief  OLED小字显示一个字符（6x8点阵）
  * @param  Line 行位置，范围：1~8
  * @param  Column 列位置，范围：1~21
  * @param  Char 要显示的字符
  * @retval 无
  */
void OLED_ShowChar_6x8(uint8_t Line, uint8_t Column, char Char)
{
    uint8_t i;
    uint8_t xStart = (Column - 1U) * 6U;
    uint8_t yPage  = Line - 1U;

    if (Line < 1U || Line > 8U || Column < 1U || Column > 21U)
    {
        return;
    }
    if (Char < ' ' || Char > '~')
    {
        Char = '?';
    }

    OLED_SetCursor(yPage, xStart);
    for (i = 0U; i < 6U; i++)
    {
        OLED_WriteData(OLED_F6x8[Char - ' '][i]);
    }
    /* 列间距填1列空白，防止字符粘连 */
    OLED_WriteData(0x00);
}

/**
  * @brief  OLED小字显示字符串（6x8点阵）
  * @param  Line 行位置，范围：1~8
  * @param  Column 列位置，范围：1~21
  * @param  String 要显示的字符串
  * @retval 无
  */
void OLED_ShowString_6x8(uint8_t Line, uint8_t Column, char *String)
{
    uint8_t i;
    for (i = 0U; String[i] != '\0' && (Column + i) <= 21U; i++)
    {
        OLED_ShowChar_6x8(Line, Column + i, String[i]);
    }
}

/**
  * @brief  OLED小字显示浮点数（6x8点阵）
  * @param  Line 行位置，范围：1~8
  * @param  Column 列位置，范围：1~21
  * @param  Num 要显示的浮点数（正数）
  * @param  IntLen 整数位数，范围：1~6
  * @param  DecLen 小数位数，范围：0~3
  * @retval 无
  */
void OLED_ShowFloat_6x8(uint8_t Line, uint8_t Column, float Num, uint8_t IntLen, uint8_t DecLen)
{
    uint8_t i;
    uint32_t intPart, decPart;

    if (Num < 0.0f) Num = 0.0f;
    if (IntLen < 1U) IntLen = 1U;
    if (IntLen > 6U)  IntLen = 6U;
    if (DecLen > 3U)  DecLen = 3U;

    intPart = (uint32_t)Num;

    /* 整数部分 */
    for (i = 0U; i < IntLen; i++)
    {
        OLED_ShowChar_6x8(Line, Column + i,
            (char)('0' + (intPart / OLED_Pow(10, IntLen - i - 1U) % 10U)));
    }

    if (DecLen > 0U)
    {
        OLED_ShowChar_6x8(Line, Column + IntLen, '.');
        /* 小数部分 */
        Num -= (float)intPart;
        for (i = 0U; i < DecLen; i++) { Num *= 10.0f; }
        decPart = (uint32_t)(Num + 0.5f);  /* 四舍五入 */
        for (i = 0U; i < DecLen; i++)
        {
            OLED_ShowChar_6x8(Line, Column + IntLen + 1U + i,
                (char)('0' + (decPart / OLED_Pow(10, DecLen - i - 1U) % 10U)));
        }
    }
}

/**
  * @brief  OLED初始化
  * @param  无
  * @retval 无
  */
void OLED_Init(void)
{
    uint32_t i, j;

    for (i = 0; i < 1000; i++)          /* 上电延时 */
    {
        for (j = 0; j < 1000; j++);
    }

    OLED_I2C_Init();                    /* 端口初始化 */

    OLED_WriteCommand(0xAE);    /* 关闭显示 */

    OLED_WriteCommand(0xD5);    /* 设置显示时钟分频比/振荡器频率 */
    OLED_WriteCommand(0x80);

    OLED_WriteCommand(0xA8);    /* 设置多路复用率 */
    OLED_WriteCommand(0x3F);

    OLED_WriteCommand(0xD3);    /* 设置显示偏移 */
    OLED_WriteCommand(0x00);

    OLED_WriteCommand(0x40);    /* 设置显示开始行 */

    OLED_WriteCommand(0xA1);    /* 设置左右方向，0xA1正常 0xA0左右反置 */

    OLED_WriteCommand(0xC8);    /* 设置上下方向，0xC8正常 0xC0上下反置 */

    OLED_WriteCommand(0xDA);    /* 设置COM引脚硬件配置 */
    OLED_WriteCommand(0x12);

    OLED_WriteCommand(0x81);    /* 设置对比度控制 */
    OLED_WriteCommand(0xCF);

    OLED_WriteCommand(0xD9);    /* 设置预充电周期 */
    OLED_WriteCommand(0xF1);

    OLED_WriteCommand(0xDB);    /* 设置VCOMH取消选择级别 */
    OLED_WriteCommand(0x30);

    OLED_WriteCommand(0xA4);    /* 设置整个显示打开/关闭 */

    OLED_WriteCommand(0xA6);    /* 设置正常/倒转显示 */

    #if (OLED_DRIVER_CHIP == OLED_DRIVER_SH1106)
    OLED_WriteCommand(0xAD);    /* SH1106：设置DC-DC */
    OLED_WriteCommand(0x8B);
    #elif (OLED_DRIVER_CHIP == OLED_DRIVER_SSD1306)
    OLED_WriteCommand(0x8D);    /* SSD1306：设置充电泵 */
    OLED_WriteCommand(0x14);
    #elif (OLED_DRIVER_CHIP == OLED_DRIVER_SSD1309)
    OLED_WriteCommand(0xAD);    /* SSD1309：设置DC-DC */
    OLED_WriteCommand(0x8B);
    #else
    #error "Unsupported OLED_DRIVER_CHIP"
    #endif

    OLED_WriteCommand(0xAF);    /* 开启显示 */

    OLED_Clear();               /* OLED清屏 */
}
