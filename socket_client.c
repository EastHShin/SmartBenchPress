#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define IN "in"
#define OUT "out"
#define LOW 0
#define HIGH 1
#define ENABLE "1"
#define UNENABLE "0"

#define POUT 18

static int GPIOExport(int pin);
static int GPIODirection(int pin, char *dir);
static int GPIOWrite(int pin, int value);
static int GPIORead(int pin);
static int GPIOUnexport(int pin);

int sock;

void error_handling(char *message) {
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}

void (*breakCapture)(int);  // ?

void signalingHandler(int signo) {
    close(sock);
    //Disable GPIO pins
    GPIOUnexport(POUT);
    exit(1);
}

int main(int argc, char *argv[]) {
    struct sockaddr_in serv_addr;

    setsid();
    umask(0);
    breakCapture = signal(SIGINT, signalingHandler);

    if (argc != 3) {
        printf("Usage : %s <IP> <port>\n", argv[0]);
        exit(1);
    }

    //Enable GPIO pins
    if (-1 == GPIOExport(POUT))
        return (1);
    //Set GPIO directions
    if (-1 == GPIODirection(POUT, OUT))
        return (2);

    sock = socket(PF_INET, SOCK_STREAM, 0);
    if (sock == -1)
        error_handling("socket() error");

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1)
        error_handling("connect() error");

    char msg[2];
    char on[2] = "1";
    while (1) {
        int str_len = read(sock, msg, sizeof(msg));
        if (str_len == -1)
            error_handling("read() error");
        printf("Receive message from Server : %s\n", msg);
        int light;
        if (strncmp(on, msg, 1) == 0) {
            light = HIGH;
            printf("h\n");
        } else {
            light = LOW;
            printf("l\n");
        }
        // usleep(1000000);
        GPIOWrite(POUT, light);
    }
    close(sock);
    //Disable GPIO pins
    if (-1 == GPIOUnexport(POUT))
        return (4);
    return (0);
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

static int GPIODirection(int pin, char *dir) {
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