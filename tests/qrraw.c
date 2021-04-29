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

extern const unsigned char data_buf[];

#define MS(ts) (unsigned int)((ts.tv_sec * 1000) + (ts.tv_nsec / 1000000))

static struct quirc *decoder;

struct result_info {
	int		file_count;
	int		id_count;
	int		decode_count;

	unsigned int	identify_time;
};

static void dump_data(const struct quirc_data *data)
{
	printf("%s", data->payload);
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

	quirc_resize(decoder, 640, 640);
	image = quirc_begin(decoder, NULL, NULL);

	memcpy(image, buf, 640*640);
	
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

static int run_tests(void)
{
	struct result_info sum;
	int count = 0;

	decoder = quirc_new();
	if (!decoder) {
		perror("quirc_new");
		return -1;
	}

	memset(&sum, 0, sizeof(sum));

	struct result_info info;

	if( scan_buffer((unsigned char*)data_buf, &info) > 0 ) {
		add_result(&sum, &info);
		count++;
	}

	quirc_destroy(decoder);
	return 0;
}

int main(int argc, char **argv)
{
	return run_tests();
}
