#ifndef 1602LCD_H
#define 1602LCD_H

void LCD_Toggle_Enable(int bits);

void LCD_Byte(int bits, int mode);
void LCD_Init();

void Write_Bytes(int output);

int LCD_Clear();

void LCD_String(char buffer[], int line);
void LCD_Shutdown();

#endif