#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>				//Needed for I2C port
#include <fcntl.h>				//Needed for I2C port
#include <sys/ioctl.h>			//Needed for I2C port
#include <linux/i2c-dev.h>		//Needed for I2C port

#define I2C_DEVICE "/dev/i2c-1"
#define I2C_ADDR 0x27
#define Enable 0b00000100 // Enable bits
#define PULSE_PERIOD 500
#define LCD_CHR  1 // Mode - Sending data
#define LCD_CMD  0 // Mode - Sending command
#define LCD_WIDTH 16 // Maximum characters per line

#define BACKLIGHT 0x08 // On
static int iBackLight = BACKLIGHT;
static int file_i2c = -1;
static int length = 1;

void LCD_Toggle_Enable(int bits){
    usleep(PULSE_PERIOD);
    Write_Bytes(bits | Enable);
    usleep(PULSE_PERIOD);
    Write_Bytes(bits & ~Enable);
    usleep(PULSE_PERIOD);
}

void LCD_Byte(int bits, int mode){
    int bits_high;
    int bits_low;

    bits_high = mode | (bits & 0xF0) | BACKLIGHT;
    bits_low = mode | ((bits << 4) & 0xF0) | BACKLIGHT;

    //High bits
    Write_Bytes(bits_high);
    LCD_Toggle_Enable(bits_high);

    //Low bits
    Write_Bytes(bits_low);
    LCD_Toggle_Enable(bits_low);
}
void LCD_Init(){
    if((file_i2c = open(I2C_DEVICE, O_RDWR)) < 0){
        printf("Failed to open the i2c bus.\n");
        return;
    }
    
    if(ioctl(file_i2c, I2C_SLAVE, I2C_ADDR) < 0){
        printf("Failed to acquire bus access and/or talk to slave.\n");
        return;
    }

    iBackLight = BACKLIGHT;

    LCD_Byte(0x33, LCD_CMD);
    LCD_Byte(0x32, LCD_CMD);
    LCD_Byte(0x06, LCD_CMD);
    LCD_Byte(0x0c, LCD_CMD);
    LCD_Byte(0x28, LCD_CMD);
    LCD_Byte(0x01, LCD_CMD);
    usleep(PULSE_PERIOD);
}

static void Write_Bytes(int output){
    if(write(file_i2c, &output, length) != length){
        printf("Failed to write to the i2c bus.\n");
        return;
    }
}

int LCD_Clear()
{
	if (file_i2c < 0)
		return 1;
	Write_Bytes(0x01); // clear the screen
	return 0;
}

void LCD_String(char buffer[], int line){
    LCD_Byte(line, LCD_CMD);

    char tmp;
    for(int i=0; i<LCD_WIDTH; i++){
        if(i >= strlen(buffer))
            tmp = ' ';
        else
            tmp = (char)buffer[i]
            
        LCD_Byte(buffer[i], LCD_CHR);
    }

}
