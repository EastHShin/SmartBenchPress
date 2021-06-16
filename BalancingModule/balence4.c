#include <arpa/inet.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define Device_Address 0x68 /*Device Address/Identifier for MPU6050*/

#define I2C_WRITE 0
#define I2C_READ 1
#define I2C_BYTE_DATA 2

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

static int PWMExport(int pin);
static int PWMEnable(int pin, char* enable);
static int PWMWritePeriod(int pin, int value);
static int PWMWriteDutyCycle(int pin, int value);
static int PWMUnexport(int pin);
void b_on(int btn_num, int tone, int dur);

int fd;
pthread_t p_thread[2];
int sock;
int clnt_sock = -1;
int clnt_sock2 = -1;
struct sockaddr_in serv_addr, clnt_addr, clnt_addr2;
int flag = 0;
int flag2 = 0;

void (*breakCapture)(int);
void signalingHandler(int signo) {
    int status;
    close(clnt_sock);
    close(clnt_sock2);
    close(sock);
    pthread_join(p_thread[0], (void**)&status);
    pthread_join(p_thread[1], (void**)&status);

    PWMWriteDutyCycle(BTN_L, 0);
    PWMWriteDutyCycle(BTN_R, 0);
    PWMEnable(0, UNENABLE);
    PWMUnexport(0);
    exit(1);
}

union i2c_smbus_data {
    unsigned char byte;
};

int setupI2C(int devId) {
    FILE* fp = popen("ls /dev/*i2c*", "r");
    if (fp < 0)
        exit(1);
    char device[11];
    // "/dev/i2c-1"
    fgets(device, sizeof(device), fp);
    pclose(fp);

    int fd;
    fd = open(device, O_RDWR);
    if (fd < 0)
        exit(1);

    if (ioctl(fd, I2C_SLAVE, devId) < 0)
        exit(1);

    return fd;
}

void writeI2C_Byte(int fd, int command, int value) {
    struct i2c_smbus_ioctl_data ioctl_data;
    ioctl_data.read_write = I2C_WRITE;
    ioctl_data.command = command;
    ioctl_data.size = I2C_BYTE_DATA;

    union i2c_smbus_data data;
    data.byte = value;
    ioctl_data.data = &data;

    ioctl(fd, I2C_SMBUS, &ioctl_data);
}

int readI2C_Byte(int fd, int command) {
    struct i2c_smbus_ioctl_data ioctl_data;
    ioctl_data.read_write = I2C_READ;
    ioctl_data.command = command;
    ioctl_data.size = I2C_BYTE_DATA;

    union i2c_smbus_data data;
    ioctl_data.data = &data;

    ioctl(fd, I2C_SMBUS, &ioctl_data);

    return data.byte & 0xFF;
}

void MPU6050_Init() {
    writeI2C_Byte(fd, SMPLRT_DIV, 0x07); /* Write to sample rate register */
    writeI2C_Byte(fd, PWR_MGMT_1, 0x01); /* Write to power management register */
    writeI2C_Byte(fd, CONFIG, 0);        /* Write to Configuration register */
    writeI2C_Byte(fd, GYRO_CONFIG, 24);  /* Write to Gyro Configuration register */
    writeI2C_Byte(fd, INT_ENABLE, 0x01); /*Write to interrupt enable register */
}
short read_raw_data(int addr) {
    short high_byte, low_byte, value;
    high_byte = readI2C_Byte(fd, addr);
    low_byte = readI2C_Byte(fd, addr + 1);
    value = (high_byte << 8) | low_byte;
    return value;
}

void buzzerInit() {
    system("gpio -g mode 18 alt5");
    system("gpio -g mode 19 alt5");

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

void* socket_read_loop1() {  // pressure
    char msg[2];
    while (1) {
        int str_len = read(clnt_sock, msg, sizeof(msg));
        if (str_len == -1)
            exit(1);
        printf("Receive message from Server1 : %s\n", msg);
        if (strncmp("1", msg, 1) == 0) {
            flag = 1;
            PWMWriteDutyCycle(BTN_L, 0);
            PWMWritePeriod(BTN_L, 5000000);
            PWMWriteDutyCycle(BTN_L, (int)(5000000 * 0.7));
            PWMWriteDutyCycle(BTN_R, 0);
            PWMWritePeriod(BTN_R, 5000000);
            PWMWriteDutyCycle(BTN_R, (int)(5000000 * 0.7));
            usleep(3000000);
            // PWMWriteDutyCycle(BTN_L, 0);
            // PWMWriteDutyCycle(BTN_R, 0);
        } else {
            PWMWriteDutyCycle(BTN_L, 0);
            PWMWriteDutyCycle(BTN_R, 0);
            flag = 0;
        }
    }
}

void* socket_read_loop2() {  // count
    char msg[2];
    while (1) {
        int str_len = read(clnt_sock2, msg, sizeof(msg));
        if (str_len == -1)
            exit(1);
        printf("Receive message from Server2 : %s\n", msg);
        if (strncmp("1", msg, 1) == 0 && flag2 == 0) {
            PWMWriteDutyCycle(BTN_L, 0);
            PWMWritePeriod(BTN_L, 15000000);
            PWMWriteDutyCycle(BTN_L, (int)(15000000 * 0.3));
            PWMWriteDutyCycle(BTN_R, 0);
            PWMWritePeriod(BTN_R, 15000000);
            PWMWriteDutyCycle(BTN_R, (int)(15000000 * 0.3));
            // usleep(100000);
            usleep(500000);
            if (flag2 == 0) {
                PWMWriteDutyCycle(BTN_L, 0);
                PWMWriteDutyCycle(BTN_R, 0);
            }
        }
    }
}

void socket_init(int port) {
    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
        exit(1);
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) == -1)
        exit(1);
    if (listen(sock, 5) == -1)
        exit(1);

    // 비상상황알림 모듈
    if (clnt_sock < 0) {
        socklen_t clnt_addr_size = sizeof(clnt_addr);
        clnt_sock = accept(sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
        printf("connected with 1\n");
        if (clnt_sock == -1)
            exit(1);
    }
    int thr_id = pthread_create(&p_thread[0], NULL, socket_read_loop1, (void*)"thread_1");
    if (thr_id < 0) {
        perror("thread create error: ");
        exit(0);
    }
    //  카운트 모듈
    if (clnt_sock2 < 0) {
        socklen_t clnt_addr_size2 = sizeof(clnt_addr2);
        clnt_sock2 = accept(sock, (struct sockaddr*)&clnt_addr2, &clnt_addr_size2);
        printf("connected with 1\n");
        if (clnt_sock2 == -1)
            exit(1);
    }

    thr_id = pthread_create(&p_thread[1], NULL, socket_read_loop2, (void*)"thread_2");
    if (thr_id < 0) {
        perror("thread create error: ");
        exit(0);
    }
}

int main(int argc, char const* argv[]) {
    float Acc_x, Acc_y, Acc_z;
    float Ax = 0, Ay = 0, Az = 0;

    if (argc != 2) {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }

    setsid();
    umask(0);
    breakCapture = signal(SIGINT, signalingHandler);

    fd = setupI2C(Device_Address); /*Initializes I2C with device Address*/
    MPU6050_Init();                /* Initializes MPU6050 */
    buzzerInit();

    socket_init(atoi(argv[1]));

    char msg[2];
    while (1) {
        /*Read raw value of Accelerometer and gyroscope from MPU6050*/
        Acc_x = read_raw_data(ACCEL_XOUT_H);
        Acc_y = read_raw_data(ACCEL_YOUT_H);
        Acc_z = read_raw_data(ACCEL_ZOUT_H);

        /* Divide raw value by sensitivity scale factor */
        Ax = Acc_x / 16384.0;
        Ay = Acc_y / 16384.0;
        Az = Acc_z / 16384.0;

        // printf("\nAx=%.3f g\tAy=%.3f g\tAz=%.3f g\n", Ax, Ay, Az);
        if (flag == 0) {
            if (Ay > 0.5) {
                // printf("right\n");
                printf("left\n");
                flag2 = 1;
                write(clnt_sock2, "1", sizeof("1"));
                b_on(BTN_L, 10000000, 10);
            } else if (Ay < -0.5) {
                printf("right\n");
                // printf("left\n");
                flag2 = 1;
                write(clnt_sock2, "1", sizeof("1"));
                b_on(BTN_R, 10000000, 10);
            } else {
                flag2 = 0;
                write(clnt_sock2, "0", sizeof("0"));
                usleep(10000);
            }
        } else {
            flag2 = 1;
            usleep(300000);
        }
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
    if (PWMWriteDutyCycle(btn_num, (int)(tone * 0.5)) == -1) {
        exit(0);
    }
    if (dur != -1) {
        usleep(dur * 1000);
        if (PWMWriteDutyCycle(btn_num, 0) == -1) {
            exit(0);
        }
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