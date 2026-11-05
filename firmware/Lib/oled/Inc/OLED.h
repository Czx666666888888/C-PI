#ifndef __OLED_H
#define __OLED_H

#include <stdint.h>

/* ── 大字 (8x16→16x32) 显示函数 ── */
void OLED_Init(void);
void OLED_Clear(void);
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char);
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String);
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length);
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);

/* ── 小字 (6x8) 显示函数 ── */
void OLED_ShowChar_6x8(uint8_t Line, uint8_t Column, char Char);
void OLED_ShowString_6x8(uint8_t Line, uint8_t Column, char *String);
void OLED_ShowFloat_6x8(uint8_t Line, uint8_t Column, float Num, uint8_t IntLen, uint8_t DecLen);

#endif
