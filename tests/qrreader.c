/* quirc -- QR-code recognition library
 * Copyright (C) 2010-2012 Daniel Beer <dlbeer@gmail.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <ctype.h>
#include <quirc.h>
#include <jpeglib.h>
#include <setjmp.h>
#include <time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <linux/input.h>

#include <linux/videodev2.h>
#include "uartFunc.h"

#define CAPTURE_IMAGE_WIDTH		640
#define CAPTURE_IMAGE_HEIGHT	480

#define MS(ts) (unsigned int)((ts.tv_sec * 1000) + (ts.tv_nsec / 1000000))

static struct quirc *decoder;

struct result_info {
	int		file_count;
	int		id_count;
	int		decode_count;

	unsigned int	identify_time;
};

unsigned char *outputBuf;
int fdUart;
int fdKey;
int fdCam;

// *****************************************************************************************************
static int xioctl(int fd, int request, void *arg)
{
        int r;
 
        do r = ioctl (fd, request, arg);
        while (-1 == r && EINTR == errno);
 
        return r;
}

int init_caps(int fd)
{
	struct v4l2_format fmt = {0};
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width = CAPTURE_IMAGE_WIDTH;
	fmt.fmt.pix.height = CAPTURE_IMAGE_HEIGHT;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_GREY;
	fmt.fmt.pix.field = V4L2_FIELD_NONE;

	if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
	{
		printf("Setting Pixel Format\r\n");
		return 1;
	}

	return 0;
}

int init_mmap(int fd)
{
    struct v4l2_requestbuffers req = {0};
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
 
    if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req))
    {
        printf("Requesting Buffer\r\n");
        return 1;
    }
 
    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;

    if(-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
    {
        printf("Querying Buffer\r\n");
        return 1;
    }
 
	outputBuf = mmap (NULL, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);

    return 0;
}

int capture_image(int fd)
{
    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;

	if(-1 == xioctl(fd, VIDIOC_QBUF, &buf))
    {
        printf("Query Buffer\n");
        return 1;
    }
 
   if(-1 == xioctl(fd, VIDIOC_STREAMON, &buf.type))
    {
        printf("Start Capture\n");
        return 1;
    }
 
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    struct timeval tv = {0};
    tv.tv_sec = 2;
    int r = select(fd+1, &fds, NULL, NULL, &tv);
    if(-1 == r)
    {
        printf("Waiting for Frame\n");
        return 1;
    }
 
    if(-1 == xioctl(fd, VIDIOC_DQBUF, &buf))
    {
        printf("Retrieving Frame\n");
        return 1;
    }

    return 0;
}

// ********************************************************************************************************

static void dump_data(const struct quirc_data *data)
{
	printf("%s(%d)", data->payload, sizeof(data->payload));
	write(fdUart, data->payload, strlen(data->payload));
	write(fdUart, "\r\n", 2);
}

static void add_result(struct result_info *sum, struct result_info *inf)
{
	sum->file_count += inf->file_count;
	sum->id_count += inf->id_count;
	sum->decode_count += inf->decode_count;

	sum->identify_time += inf->identify_time;
}

static int scan_buffer(unsigned char *buf, struct result_info *info)
{
	struct timespec tp;
	unsigned int start;
	int i;

	unsigned char *image;

	quirc_resize(decoder, CAPTURE_IMAGE_WIDTH, CAPTURE_IMAGE_HEIGHT);
	image = quirc_begin(decoder, NULL, NULL);

	memcpy(image, buf, CAPTURE_IMAGE_WIDTH*CAPTURE_IMAGE_HEIGHT);
	
		(void)clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tp);
		start = MS(tp);
	quirc_end(decoder);
		(void)clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tp);
		info->identify_time = MS(tp) - start;

	info->id_count = quirc_count(decoder);
	for (i = 0; i < info->id_count; i++) {
		struct quirc_code code;
		struct quirc_data data;

		quirc_extract(decoder, i, &code);

		quirc_decode_error_t err = quirc_decode(&code, &data);
		if (err == QUIRC_ERROR_DATA_ECC) {
			quirc_flip(&code);
			err = quirc_decode(&code, &data);
		}

		if (!err) {
			info->decode_count++;
		}
	}

	(void)clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tp);

	printf("ID Time = %u\n", info->identify_time);
	printf("Count = %d\n", info->id_count);

	for (i = 0; i < info->id_count; i++) {
		struct quirc_code code;

		quirc_extract(decoder, i, &code);

		struct quirc_data data;
		quirc_decode_error_t err = quirc_decode(&code, &data);
		if (err == QUIRC_ERROR_DATA_ECC) {
			quirc_flip(&code);
			err = quirc_decode(&code, &data);
		}

		if (err) {
			printf("  ERROR: %s\n\n", quirc_strerror(err));
		} else {
			printf("Decode successful:\n");
			dump_data(&data);
			printf("\n");
		}
	}

	info->file_count = 1;
	return 1;
}

int camOpen(char *camDevice)
{
	fdCam = open(camDevice, O_RDWR);
    if (fdKey == -1)
    {
            printf("Opening video device\r\n");
            return 1;
    }

	if( init_caps(fdCam) )
	{
        return 1;
	}    

    if(init_mmap(fdCam))
	{
        return 1;
	}

	return 0;
}

int camRetrive()
{
    capture_image(fdCam);

	return 0;
}

static int run_tests(void)
{
	struct result_info sum;
	int count = 0;
	struct input_event t;

	camOpen("/dev/video0");	

	decoder = quirc_new();
	if (!decoder) {
		perror("quirc_new");
		return -1;
	}

	while(1)
	{

		if( read(fdKey, &t, sizeof(t)) == sizeof(t) )
		{
			if( (t.type == EV_KEY) && (t.value == 1) && (t.code == 148) )
			{
				camRetrive();
				memset(&sum, 0, sizeof(sum));

				struct result_info info;

				if( scan_buffer(outputBuf, &info) > 0 ) {
					add_result(&sum, &info);
					count++;
				}
			}
		}

	}

	quirc_destroy(decoder);
	return 0;
}

void led_on(void)
{
	int fd;

	if( (fd = open("/sys/class/leds/green/brightness", O_WRONLY)) < 0 )
	{
		printf("Can't open led\r\n");
		return -1;
	}

	write(fd, "1", 1);

	close(fd);
}

int main(int argc, char **argv)
{
	char *dev  = "/dev/ttyS2";
	fdUart = uartOpen(dev);
	uartSetSpeed(fdUart, 115200);

	if( (fdKey = open("/dev/input/event0", O_RDONLY)) <= 0 )
	{
		printf("open event error\r\n");
		return -1;
	}


	led_on();	

	run_tests();

	close(fdCam);
	close(fdKey);
	close(fdUart);

	return 0;
}
