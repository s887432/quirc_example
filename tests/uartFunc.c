#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <string.h>

// ******************************************
// internal data
// ******************************************
int speed_arr[] = { B115200, B57600, B38400, B19200, B9600, B4800, B2400, B1200, B300,
		B38400, B19200, B9600, B4800, B2400, B1200, B300, };

int name_arr[] = {115200, 57600, 38400,  19200,  9600,  4800,  2400,  1200,  300,
		38400,  19200,  9600, 4800, 2400, 1200,  300, };

// ******************************************
// extern functions
// ******************************************
int uartOpen(char *Dev)
{
	int fd = open( Dev, O_RDWR );
	struct termios  tty;
	int rc;

	if (-1 == fd)   
	{
		perror("Can't Open Serial Port");
		return -1;              
	}

	rc = tcgetattr(fd, &tty);
    if (rc < 0) {
        printf("failed to get attr: %d, %s\r\n", rc, strerror(errno));
        return -1;
    }

    cfmakeraw(&tty);

    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 10;

    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;    /* no HW flow control? */
    tty.c_cflag |= CLOCAL | CREAD;
    rc = tcsetattr(fd, TCSANOW, &tty);
    if (rc < 0) {
        printf("failed to set attr: %d, %s\r\n", rc, strerror(errno));
        return -1;
    }

	return fd;
}

void uartSetSpeed(int fd, int speed)
{
	int   i;
	int   status;
	struct termios   Opt;
	
	tcgetattr(fd, &Opt);

	for ( i= 0;  i < sizeof(speed_arr) / sizeof(int);  i++)
	{
		if  (speed == name_arr[i])
		{
			tcflush(fd, TCIOFLUSH);
			cfsetispeed(&Opt, speed_arr[i]);
			cfsetospeed(&Opt, speed_arr[i]);
			status = tcsetattr(fd, TCSANOW, &Opt);
	
			if  (status != 0)
			{
				perror("tcsetattr fd1");
			}

			return;
		}

		tcflush(fd,TCIOFLUSH);
	}
}

int uartSetParity(int fd,int databits,int stopbits,int parity)
{
	struct termios options;

	if  ( tcgetattr( fd,&options)  !=  0)
	{
		perror("SetupSerial 1");
		return(-1);
	}

	options.c_cflag &= ~CSIZE;

	switch (databits)
	{
		case 7:
			options.c_cflag |= CS7;
			break;

		case 8:
			options.c_cflag |= CS8;
			break;

		default:
			fprintf(stderr,"Unsupported data size\n");
			return (-1);
	}

	switch (parity)
	{
		case 'n':
		case 'N':
			options.c_cflag &= ~PARENB;
			options.c_iflag &= ~INPCK;
			break;

		case 'o':
		case 'O':
			options.c_cflag |= (PARODD | PARENB);
			options.c_iflag |= INPCK;
			break;

		case 'e':
		case 'E':
			options.c_cflag |= PARENB;
			options.c_cflag &= ~PARODD;
			options.c_iflag |= INPCK;
			break;

		case 'S':
		case 's':
			options.c_cflag &= ~PARENB;
			options.c_cflag &= ~CSTOPB;
			break;

		default:
			fprintf(stderr,"Unsupported parity\n");
			return (-1);
	}

	switch (stopbits)
	{
		case 1:
			options.c_cflag &= ~CSTOPB;
			break;

		case 2:
			options.c_cflag |= CSTOPB;
			break;

		default:
			fprintf(stderr,"Unsupported stop bits\n");
			return (-1);
	}

	if (parity != 'n')
	{
		options.c_iflag |= INPCK;
	}

	options.c_cc[VTIME] = 150;
    options.c_cc[VMIN] = 0;

	tcflush(fd,TCIFLUSH);

	if (tcsetattr(fd,TCSANOW,&options) != 0)
	{
		perror("SetupSerial 3");
		return -1;
	}

	return (0);
}

// end of file

