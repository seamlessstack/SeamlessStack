/*
 * Copyright (C) 2014 SeamlessStack
 *
 *  This file is part of SeamlessStack distributed file system.
 *
 * SeamlessStack distributed file system is free software: you can redistribute
 * it and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 *
 * SeamlessStack distributed file system is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SeamlessStack distributed file system. If not,
 * see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

/*
 * To try this, copy sstack_compression_plugin.c to this directory.
 * Comment out policy.h inclusion
 * Modify input and output file names.
 * cc -g example.c sstack_compression_plugin.c -o example -llzma
 */

extern size_t compression_plugin_compress(char *, char *,size_t );
extern size_t compression_plugin_decompress(char *, char *,size_t );

int main(void)
{
	int fd = -1;
	struct stat stbuf = { '\0' };
	char *buffer = NULL;
	int ret = -1;
	char *outbuffer = NULL;
	size_t realsize = 0;

	fd = open("/home/anand/input_file", O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "%s: Failed to open file %s \n", __FUNCTION__,
						"/home/anand/input_file");
		perror(" ");
		return -1;
	}
	ret = stat("/home/anand/input_file", &stbuf);
	if (ret == -1) {
		fprintf(stderr, "%s: stat failed \n", __FUNCTION__);
		close(fd);
		return -1;
	}

	buffer = malloc(stbuf.st_size);
	if (NULL == buffer) {
		fprintf(stderr, "%s: Failed to allocate memory \n", __FUNCTION__);
		close(fd);
		return -1;
	}
	outbuffer = malloc(stbuf.st_size);
	if (NULL == buffer) {
		fprintf(stderr, "%s: Failed to allocate memory \n", __FUNCTION__);
		close(fd);
		return -1;
	}
	ret = read(fd, buffer, stbuf.st_size);
	if (ret == -1) {
		fprintf(stderr, "%s: read failed \n", __FUNCTION__);
		close(fd);
		return -1;
	}

	close(fd);
	realsize = compression_plugin_compress(buffer, outbuffer, stbuf.st_size);
	fprintf(stderr, "%s: Realsize after compression = %d \n",
					__FUNCTION__, (int) realsize);


	// Uncompress the file back
	realsize = compression_plugin_decompress(outbuffer, buffer, realsize);
	fprintf(stderr, "%s: Realsize after decompression = %d \n",
					__FUNCTION__, (int) realsize);

	// Write it to output file
	fd = open("/home/anand/output", O_CREAT | O_RDWR, 0777);
	if (fd == -1) {
		fprintf(stderr, "%s: Failed to open file %s \n", __FUNCTION__,
						"/home/anand/output");
		perror(" ");
		return -1;
	}

	ret = write(fd, buffer, realsize);
	if (ret == -1) {
		fprintf(stderr, "%s: write failed \n", __FUNCTION__);
		close(fd);
		return -1;
	}

	close(fd);

	return 0;
}
