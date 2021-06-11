#ifndef LCD1602_H
#define LCD1602_H

//
int lcd1602Init(int iChannel, int iAddr);

//
// Set the cursor position on the LCD
//
int lcd1602SetCursor(int x, int y);

//
// Control the backlight, cursor, and blink
//
int lcd1602Control(int bBacklight, int bCursor, int bBlink);

//
// Print a zero-terminated character string
// Only 1 line at a time can be accessed
//
int lcd1602WriteString(char *szText);

//
// Clear the characters from the LCD
//
int lcd1602Clear(void);

//
// Turn off the LCD and backlight
// close the I2C file handle
//
void lcd1602Shutdown(void);

#endif  // LCD1602_H
