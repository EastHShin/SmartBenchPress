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

#define	IN 0
#define	OUT 1
#define	PWM 2
#define	LOW 0
#define	HIGH 1
#define	PIN 20
#define	POUT 21
#define	VALUE_MAX 256

#define	ARRAY_SIZE(array) sizeof(array) / sizeof(array[0])

static const char *DEVICE = "/dev/spidev0.0";
static uint8_t MODE = SPI_MODE_0;
static uint8_t BITS = 8;
static uint32_t CLOCK = 1000000;
static uint16_t DELAY = 5;
int serv_sock =1 ;

void error_handling(char *message){
fputs(message,stderr);
fputc('\n',stderr);
exit(1);
}

//SPI 
static int prepare(int fd) {
	if (ioctl(fd, SPI_IOC_WR_MODE, &MODE) == -1) {
		perror("Can`t set MODE");
		return -1;
	}
	
	if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &BITS) == -1) {
		perror("Can`t set number of BITS");
		return -1;
	}
	
	if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &CLOCK) == -1) {
		perror("Can`t set write CLOCK");
		return -1;
	}
	
	if (ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &CLOCK) == -1) {
		perror("Can`t set read CLOCK");
		return -1;
	}
	
	return 0;
}

uint8_t control_bits_differential(uint8_t channel) {
	return (channel & 7) << 4;
}

uint8_t control_bits(uint8_t channel) {
	return 0x8 | control_bits_differential(channel);
}

int readadc(int fd, uint8_t channel) {
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
	
	if (ioctl(fd, SPI_IOC_MESSAGE(1), &tr) == 1) {
		perror("IO Error");
		abort();
	}
	
	return ((rx[1] << 8) & 0x300) | (rx[2] & 0xFF);
}

void (*breakCapture)(int);

void signalingHandler(int signo) {
	close(serv_sock);
	exit(1);
}

int main(int argc, char **argv) {
	//ctrl+c
	setsid();
	umask(0);
	
	breakCapture = signal(SIGINT, signalingHandler);
	
	//pressure sensor
	int pressure = 0;
	int flag = 0;
	int count = 0;
	
	int fd = open(DEVICE, O_RDWR);
	if (fd <= 0) {
		printf("Device %s not found\n", DEVICE);
		return -1;
	}
	
	if (prepare(fd) == -1) {
		return -1;
	}
	
	while(flag!=1) {
		pressure = readadc(fd,0);
		printf("this is pressure value %d\n",pressure);
		
		if (pressure >= 100) {
			count++;
			printf("this is count value %d\n",count);
		}
		if (flag == 1 && count>=2) {//reset
			printf("flag Reset\n");
			count=0;
			flag=0;
		}
		if (flag == 0 && count==2) {//set
			printf("flag Set\n",flag);
			flag=1;
		}
		printf("this is 2sec, check %d\n",flag);
		usleep(1000000);
	}
	close(fd);
	
	//Server
	int state = 1;
	int prev_state = 1;
	int light = 0;
	int clnt_sock=-1;
	struct sockaddr_in serv_addr,clnt_addr;
	socklen_t clnt_addr_size;
	char msg[2];

	if(argc!=2){
		printf("Usage : %s <port>\n",argv[0]);
	}
	
	serv_sock = socket(PF_INET, SOCK_STREAM, 0);
	if(serv_sock == -1)
		error_handling("socket() error");
		
	memset(&serv_addr, 0 , sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(atoi(argv[1]));
	
	if(bind(serv_sock, (struct sockaddr*) &serv_addr, sizeof(serv_addr))==-1)
		error_handling("bind() error");
	
	if(listen(serv_sock,5) == -1)
		error_handling("listen() error");

	if(clnt_sock<0){
		clnt_addr_size = sizeof(clnt_addr);
		clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);
		if(clnt_sock == -1)
			error_handling("accept() error");
	}
	while(1)
	{
		//state = GPIORead(PIN);
		if(prev_state == 0 && state == 1){
			//light = (light+1)%2;
			snprintf(msg,2,"%d",flag);
			write(clnt_sock, msg, sizeof(msg));
			printf("msg = %s\n",msg);
			close(clnt_sock);
			clnt_sock = -1;
		}
		prev_state = state;
		usleep(500 * 100);
	}
	return 0;
}
