/*************************************************************************
 *
 * SEAMLESSSTACK CONFIDENTIAL
 * __________________________
 *
 *  [2012] - [2014]  SeamlessStack Inc
 *  All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains
 * the property of SeamlessStack Incorporated and its suppliers,
 * if any.  The intellectual and technical concepts contained
 * herein are proprietary to SeamlessStack Incorporated
 * and its suppliers and may be covered by U.S. and Foreign Patents,
 * patents in process, and are protected by trade secret or copyright law.
 * Dissemination of this information or reproduction of this material
 * is strictly forbidden unless prior written permission is obtained
 * from SeamlessStack Incorporated.
 */

#include <stdio.h>
#include <zlib.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include <sstack_log.h>


extern uint32_t crc_pcl(const char * file_buffer, size_t file_size,
			uint32_t prev);

uint32_t
sstack_checksum(log_ctx_t *ctx, const char *filename)
{
	int fd = -1;
	char *file_buffer;
	uint64_t file_size;
	uint64_t bytes_read;
	uint32_t crc;
	struct stat statbuf;
	int res = -1;

	fd = open(filename, O_RDWR);
	if (fd == -1) {
		sfs_log(ctx, SFS_ERR, "%s: Not able to open %s for reading \n",
			__FUNCTION__, filename);
		return errno;
	}
	res  = stat(filename, &statbuf);
	if (res != 0) {
		sfs_log(ctx, SFS_ERR, "%s: stat failed on %s \n",
			__FUNCTION__, filename);
		close(fd);
		return errno;
	}

	file_size = statbuf.st_size;

	if( file_size == 0 ) {
		sfs_log(ctx, SFS_INFO, "%s: File size of %s is 0. No CRC calculated\n",
			 __FUNCTION__, filename);
		close(fd);
		return 0;
	}

	file_buffer = (char *) malloc(file_size);
	if (NULL == file_buffer) {
		sfs_log(ctx, SFS_ERR, "%s: Unable to allocate memory. File %s \n",
			__FUNCTION__, filename);
		close(fd);

		return errno;
	}

	bytes_read = read(fd, file_buffer, file_size );

	if (bytes_read < file_size) {
		sfs_log(ctx,SFS_ERR , "%s: Unable to read the entire file %s. "
			"Number of bytes read is %"PRId64" CRC is not calculated "
			"properly \n", __FUNCTION__, filename, bytes_read);
		free(file_buffer);
		close(fd);
		return 0;
	}

#ifdef ZLIB_CRC
	crc = crc32(0L, Z_NULL, 0);
	crc = crc32(crc, (const Bytef *) file_buffer, file_size);
#elif SSE4_CRC
	crc = crc_pcl((const char *) file_buffer, file_size, 0);
#else
	crc = crcfast((const char *) file_buffer, file_size);
#endif

	sfs_log(ctx, SFS_INFO, "%s: Calculated CRC for %s is %u \n",
			__FUNCTION__, filename, crc);

	free(file_buffer);
	close(fd);

	return crc;
}
