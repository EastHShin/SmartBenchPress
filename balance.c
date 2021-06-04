/*
	MPU6050 Interfacing with Raspberry Pi
	http://www.electronicwings.com
*/

#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <wiringPi.h>
#include <wiringPiI2C.h>

#define Device_Address 0x68 /*Device Address/Identifier for MPU6050*/

#define PWR_MGMT_1 0x6B
#define SMPLRT_DIV 0x19
#define CONFIG 0x1A
#define GYRO_CONFIG 0x1B
#define INT_ENABLE 0x38
#define ACCEL_XOUT_H 0x3B
#define ACCEL_YOUT_H 0x3D
#define ACCEL_ZOUT_H 0x3F
#define GYRO_XOUT_H 0x43
#define GYRO_YOUT_H 0x45
#define GYRO_ZOUT_H 0x47

#define IN "in"
#define OUT "out"
#define LOW 0
#define HIGH 1
#define ENABLE "1"
#define UNENABLE "0"

#define BTN_L 0
#define BTN_R 1

static int GPIOExport(int pin);
static int GPIODirection(int pin, char* dir);
static int GPIOWrite(int pin, int value);
static int GPIORead(int pin);
static int GPIOUnexport(int pin);

static int PWMExport(int pin);
static int PWMEnable(int pin, char* enable);
static int PWMWritePeriod(int pin, int value);
static int PWMWriteDutyCycle(int pin, int value);
static int PWMUnexport(int pin);
void b_on(int btn_num, int tone, int dur);

int fd;
int sock;
pthread_t socket_thread;
int flag = 0;

void (*breakCapture)(int);
void signalingHandler(int signo) {
    int status;
    pthread_join(socket_thread, (void**)&status);
    PWMEnable(0, UNENABLE);
    PWMUnexport(0);
    exit(1);
}

void MPU6050_Init() {
    wiringPiI2CWriteReg8(fd, SMPLRT_DIV, 0x07); /* Write to sample rate register */
    wiringPiI2CWriteReg8(fd, PWR_MGMT_1, 0x01); /* Write to power management register */
    wiringPiI2CWriteReg8(fd, CONFIG, 0);        /* Write to Configuration register */
    wiringPiI2CWriteReg8(fd, GYRO_CONFIG, 24);  /* Write to Gyro Configuration register */
    wiringPiI2CWriteReg8(fd, INT_ENABLE, 0x01); /*Write to interrupt enable register */
}
short read_raw_data(int addr) {
    short high_byte, low_byte, value;
    high_byte = wiringPiI2CReadReg8(fd, addr);
    low_byte = wiringPiI2CReadReg8(fd, addr + 1);
    value = (high_byte << 8) | low_byte;
    return value;
}

void buzzerInit() {
    if (PWMExport(BTN_L) == -1 || PWMExport(BTN_R) == -1) {
        exit(0);
    }
    if (PWMWritePeriod(BTN_L, 10000000) == -1 || PWMWritePeriod(BTN_R, 10000000) == -1) {
        exit(0);
    }
    if (PWMWriteDutyCycle(BTN_L, 0) == -1 || PWMWriteDutyCycle(BTN_R, 0) == -1) {
        exit(0);
    }
    if (PWMEnable(BTN_L, ENABLE) == -1 || PWMEnable(BTN_R, ENABLE) == -1) {
        exit(0);
    }
}
void* socket_client(char const* argv[]) {
    struct sockaddr_in serv_addr;

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket() error");
        exit(1);
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1) {
        perror("socket() error");
        exit(1);
    }

    char msg[2];
    char on[2] = "1";
    while (1) {
        int str_len = read(sock, msg, sizeof(msg));
        if (str_len == -1)
            eixt(1);
        printf("Receive message from Server : %s\n", msg);
        if (strncmp(on, msg, 1) == 0) {
            flag = 1;
            b_on(BTN_L, 5000000, 3000);
            b_on(BTN_R, 5000000, 3000);
            flag = 0;
        }
    }
    close(sock);
}

int main(int argc, char const* argv[]) {
    if (argc != 3) {
        printf("Usage : %s <IP> <port>\n", argv[0]);
        exit(1);
    }

    int thr_id = pthread_create(&socket_thread, NULL, socket_client, (void*)argv);
    if (thr_id < 0) {
        perror("thread create error: ");
        exit(0);
    }

    float Acc_x, Acc_y, Acc_z;
    float Ax = 0, Ay = 0, Az = 0;

    setsid();
    umask(0);
    breakCapture = signal(SIGINT, signalingHandler);

    fd = wiringPiI2CSetup(Device_Address); /*Initializes I2C with device Address*/
    MPU6050_Init();                        /* Initializes MPU6050 */
    buzzerInit();

    while (1) {
        /*Read raw value of Accelerometer and gyroscope from MPU6050*/
        Acc_x = read_raw_data(ACCEL_XOUT_H);
        Acc_y = read_raw_data(ACCEL_YOUT_H);
        Acc_z = read_raw_data(ACCEL_ZOUT_H);

        /* Divide raw value by sensitivity scale factor */
        Ax = Acc_x / 16384.0;
        Ay = Acc_y / 16384.0;
        Az = Acc_z / 16384.0;

        // printf("\n Gx=%.3f °/s\tGy=%.3f °/s\tGz=%.3f °/s\tAx=%.3f g\tAy=%.3f g\tAz=%.3f g\n", Gx, Gy, Gz, Ax, Ay, Az);
        printf("\nAx=%.3f g\tAy=%.3f g\tAz=%.3f g\n", Ax, Ay, Az);
        if (flag == 0) {
            if (Ay > 0.3) {
                printf("left\n");
                b_on(BTN_L, 10000000, 1000);
            } else if (Ay < -0.3) {
                printf("right\n");
                b_on(BTN_R, 10000000, 1000);
            }
        }
        delay(500);
    }
    return 0;
}

void b_on(int btn_num, int tone, int dur) {  // ms
    if (PWMWriteDutyCycle(btn_num, 0) == -1) {
        exit(0);
    }
    if (PWMWritePeriod(btn_num, tone) == -1) {
        exit(0);
    }
    if (PWMWriteDutyCycle(btn_num, (int)(tone * 0.9)) == -1) {
        exit(0);
    }

    if (dur != -1) {
        usleep(dur * 1000);
        if (PWMWriteDutyCycle(btn_num, 0) == -1) {
            exit(0);
        }
        // usleep(100000);
    }
}

void b_off(int btn_num) {
    if (PWMWriteDutyCycle(btn_num, 0) == -1) {
        exit(0);
    }
}

static int PWMExport(int pin) {
    int fd = open("/sys/class/pwm/pwmchip0/export", O_WRONLY);
    if (fd == -1) {
        fprintf(stderr, "Failed to open export for writing\n");
        return -1;
    }

    const int BUFFER_MAX = 3;
    char buffer[BUFFER_MAX];
    int bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
    write(fd, buffer, bytes_written);
    close(fd);
    usleep(50 * 1000);  // inital setting하는데 시간이 걸릴 것이다.
    return 0;
}

static int PWMEnable(int pin, char* enable) {
    const int PATH_MAX = 37;
    char path[PATH_MAX];
    snprintf(path, PATH_MAX, "/sys/class/pwm/pwmchip0/pwm%d/enable", pin);
    int fd = open(path, O_WRONLY);
    if (fd == -1) {
        fprintf(stderr, "Failed to open pwm enable for writing\n");
        return -1;
    }

    if (write(fd, enable, strlen(enable)) == -1) {
        fprintf(stderr, "Failed to set enable\n");
        return -1;
    }
    close(fd);
    return 0;
}

static int PWMWritePeriod(int pin, int value) {
    const int PATH_MAX = 37;
    char path[PATH_MAX];
    snprintf(path, PATH_MAX, "/sys/class/pwm/pwmchip0/pwm%d/period", pin);
    int fd = open(path, O_WRONLY);
    if (fd == -1) {
        fprintf(stderr, "Failed to open pwm period for writing\n");
        return -1;
    }

    const int VALUE_MAX = 50;
    char value_str[VALUE_MAX];
    int byte = snprintf(value_str, VALUE_MAX, "%d", value);
    if (write(fd, value_str, byte) == -1) {
        fprintf(stderr, "Failed to write period\n");
        return -1;
    }
    close(fd);
    return value;
}

static int PWMWriteDutyCycle(int pin, int value) {
    const int PATH_MAX = 41;
    char path[PATH_MAX];
    snprintf(path, PATH_MAX, "/sys/class/pwm/pwmchip0/pwm%d/duty_cycle", pin);
    int fd = open(path, O_WRONLY);
    if (fd == -1) {
        fprintf(stderr, "Failed to open pwm duty_cycle for writing\n");
        return -1;
    }

    const int VALUE_MAX = 50;
    char value_str[VALUE_MAX];
    int byte = snprintf(value_str, VALUE_MAX, "%d", value);
    if (write(fd, value_str, byte) == -1) {
        fprintf(stderr, "Failed to write duty_cycle\n");
        return -1;
    }
    close(fd);
    return value;
}

static int PWMUnexport(int pin) {
    int fd = open("/sys/class/pwm/pwmchip0/unexport", O_WRONLY);
    if (fd == -1) {
        fprintf(stderr, "Failed to open unexport for writing\n");
        return -1;
    }

    const int BUFFER_MAX = 3;
    char buffer[BUFFER_MAX];
    int bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
    write(fd, buffer, bytes_written);

    close(fd);
    return 0;
}

static int GPIOExport(int pin) {
    int fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd == -1) {
        fprintf(stderr, "Failed to open export for writing\n");
        return -1;
    }

    const int BUFFER_MAX = 3;
    char buffer[BUFFER_MAX];
    int bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
    write(fd, buffer, bytes_written);
    close(fd);
    usleep(50 * 1000);  // direction파일이 chmod되는데 시간이 걸린다.
    return 0;
}

static int GPIODirection(int pin, char* dir) {
    const int PATH_MAX = 33;
    char path[PATH_MAX];
    snprintf(path, PATH_MAX, "/sys/class/gpio/gpio%d/direction", pin);

    int fd = open(path, O_WRONLY);
    if (fd == -1) {
        fprintf(stderr, "Failed to open gpio direction for writing\n");
        return -1;
    }

    if (write(fd, dir, strlen(dir)) == -1) {
        fprintf(stderr, "Failed to set direction\n");
        return -1;
    }
    close(fd);
    return 0;
}

static int GPIOWrite(int pin, int value) {
    const int PATH_MAX = 30;
    char path[PATH_MAX];
    snprintf(path, PATH_MAX, "/sys/class/gpio/gpio%d/value", pin);

    int fd = open(path, O_WRONLY);
    if (fd == -1) {
        fprintf(stderr, "Failed to open gpio value for writing\n");
        return -1;
    }

    char value_str[2];
    snprintf(value_str, 2, "%d", value);
    if (write(fd, value_str, 1) == -1) {
        fprintf(stderr, "Failed to write value\n");
        return -1;
    }
    close(fd);
    return value;
}

static int GPIORead(int pin) {
    const int PATH_MAX = 30;
    char path[PATH_MAX];
    snprintf(path, PATH_MAX, "/sys/class/gpio/gpio%d/value", pin);

    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Failed to open gpio value for reading\n");
        return -1;
    }

    char value_str[3];
    if (read(fd, value_str, 3) == -1) {
        fprintf(stderr, "Failed to read value\n");
        return -1;
    }
    close(fd);
    return atoi(value_str);
}

static int GPIOUnexport(int pin) {
    int fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if (fd == -1) {
        fprintf(stderr, "Failed to open unexport for writing\n");
        return -1;
    }

    const int BUFFER_MAX = 3;
    char buffer[BUFFER_MAX];
    int bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
    write(fd, buffer, bytes_written);

    close(fd);
    return 0;
}