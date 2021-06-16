#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>

#define IN 0
#define OUT 1
#define PWM 2
#define LOW 0
#define HIGH 1
#define PIN 20
#define POUT 21
#define VALUE_MAX 256

#define ARRAY_SIZE(array) sizeof(array) / sizeof(array[0])

static const char *DEVICE = "/dev/spidev0.0";
static uint8_t MODE = SPI_MODE_0;
static uint8_t BITS = 8;
static uint32_t CLOCK = 1000000;
static uint16_t DELAY = 5;
int client_sock = 1;

void error_handling(char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

//SPI
static int prepare(int fd)
{
    if (ioctl(fd, SPI_IOC_WR_MODE, &MODE) == -1)
    {
        perror("Can`t set MODE");
        return -1;
    }

    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &BITS) == -1)
    {
        perror("Can`t set number of BITS");
        return -1;
    }

    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &CLOCK) == -1)
    {
        perror("Can`t set write CLOCK");
        return -1;
    }

    if (ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &CLOCK) == -1)
    {
        perror("Can`t set read CLOCK");
        return -1;
    }

    return 0;
}

uint8_t control_bits_differential(uint8_t channel)
{
    return (channel & 7) << 4;
}

uint8_t control_bits(uint8_t channel)
{
    return 0x8 | control_bits_differential(channel);
}

int readadc(int fd, uint8_t channel)
{
    uint8_t tx[] = {1, control_bits(channel), 0};
    uint8_t rx[3];

    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = (unsigned long)rx,
        .len = ARRAY_SIZE(tx),
        .delay_usecs = DELAY,
        .speed_hz = CLOCK,
        .bits_per_word = BITS,
    };

    if (ioctl(fd, SPI_IOC_MESSAGE(1), &tr) == 1)
    {
        perror("IO Error");
        abort();
    }

    return ((rx[1] << 8) & 0x300) | (rx[2] & 0xFF);
}

void (*breakCapture)(int);

void signalingHandler(int signo)
{
    close(client_sock);
    exit(1);
}

int main(int argc, char **argv)
{
    //ctrl+c
    setsid();
    umask(0);

    breakCapture = signal(SIGINT, signalingHandler);
    //press
    int flag = 0;
    int pressure = 0;
    int count = 0;
    int check_flag = -1;

    //client
    char msg[2];
    char on[2] = "1";
    struct sockaddr_in serv_addr, clnt_addr;
    socklen_t clnt_addr_size;

    client_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (client_sock == -1)
        error_handling("socket() error");
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));
    if (connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect() error");

    int fd = open(DEVICE, O_RDWR);
    if (fd <= 0)
    {
        printf("Device %s not found\n", DEVICE);
        return -1;
    }

    if (prepare(fd) == -1)
    {
        return -1;
    }
    if (argc != 3)
    {
        printf("Usage : %s <IP> <port>\n", argv[0]);
        exit(1);
    }

    while (1)
    {
        pressure = readadc(fd, 0);
        printf("this is pressure value %d\n", pressure);

        if (pressure >= 10)
        {
            count++;
            printf("this is count value %d\n", count);
        }
        if (flag == 1 && count >= 20 && pressure < 10)
        { //reset
            printf("flag Reset\n");
            count = 0;
            flag = 0;
            check_flag = 1;
        }
        if (flag == 0 && count == 20)
        { //set
            printf("flag Set\n", flag);
            flag = 1;
            check_flag = 1;
        }

        if (check_flag == 1)
        {
            snprintf(msg, 2, "%d", flag);
            check_flag = -1;
            write(client_sock, msg, sizeof(msg));
            printf("msg = %s\n", msg);
        }
        usleep(100000);
    }
    close(client_sock);

    close(fd);

    return 0;
}
