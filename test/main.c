/*
 * main.c

 *
 *  Created on: 30 set 2015
 *      Author: ntonjeta
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>

/* My Arduino is on /dev/ttyACM0 */
char *portname = "/dev/ttyACM0";
char *r="a";

void configure  (int *fd);
void singleMode (int fd,char * jstring);
void rawMode 	(int fd,char * jstring);


int main (int argc, char* argv[]){
	int fd,n;
	char response[256];

	/* Open the file descriptor in non-blocking mode */
	if(fd = open(portname,O_RDWR | O_NOCTTY | O_NONBLOCK)){
		printf("stream aperto\n");
	}else{
		printf("errora nell'apertura dello stream\n");
		return 0;
	}
	/* Set up the control structure */
	struct termios toptions;

	 /* Get currently set options for the tty */
	tcgetattr(fd, &toptions);

	/* Set custom options */

	/* 9600 baud */
	cfsetispeed(&toptions, B9600);
	cfsetospeed(&toptions, B9600);
	/* 8 bits, no parity, no stop bits */
	toptions.c_cflag &= ~PARENB;
	toptions.c_cflag &= ~CSTOPB;
	toptions.c_cflag &= ~CSIZE;
	toptions.c_cflag |= CS8;
	/* no hardware flow control */
	toptions.c_cflag &= ~CRTSCTS;
	/* enable receiver, ignore status lines */
	toptions.c_cflag |= CREAD | CLOCAL;
	/* disable input/output flow control, disable restart chars */
	toptions.c_iflag &= ~(IXON | IXOFF | IXANY);
	/* disable canonical input, disable echo,
	disable visually erase chars,
	disable terminal-generated signals */
	toptions.c_iflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	/* disable output processing */
	toptions.c_oflag &= ~OPOST;

	/* wait for 24 characters to come in before read returns */
	toptions.c_cc[VMIN] = 12;
	/* no minimum time to wait before read returns */
	toptions.c_cc[VTIME] = 0;

	/* commit the options */
	tcsetattr(fd, TCSANOW, &toptions);

	/* Wait for the Arduino to reset */
	usleep(1000*1000);

	/* Flush anything already in the serial buffer */
	tcflush(fd, TCIFLUSH);

	char *jstring = "{ \"command\" : \"read\",\"ID\" : 1}\n";
	//Write on COM port
	rawMode(fd,jstring);

	tcflush(fd,TCIOFLUSH);

	/*Wait the board build a answer */
	usleep(1000*1000);
	
        char buf[]="\0";
//	while(n=read(fd,&buf,sizeof(char)) == -1){
//		printf("code : %d",n);
//	}
	//if (read(fd,&buf,sizeof(char)) == 0) printf("risposta : %s",buf);
	//else printf("Error %s",buf);
        
        while(read(fd,&buf,sizeof(char)) == -1);
        printf("risposta: %c", *buf);

        //read(fd,buf,1);
        //printf("risposta: %c", *buf);
        return 0;

}

//Write 1 bit for time
void singleMode(int fd,char * jstring){
	int i = 0;
	char buf = '\0';
	while(buf != '\n'){
		buf = jstring[i];
		write(fd,&buf,1);
		usleep(26*100);
		i++;
	}
}

void rawMode (int fd,char * jstring){
	write(fd,jstring,strlen(jstring));
	usleep((25+strlen(jstring))*100);
}
//
//void USB_read (int *fd,char *buf)
//{
//	/* read up to 128 bytes from the fd */
//
//	// /* print how many bytes read */
//	// printf("%i bytes got read...\n", n);
//	//  print what's in the buffer
//	// printf("Buffer contains...\n%s\n", buf);
//}
//
//void USB_write(int *fd,char *buf ){
//
//}
//
