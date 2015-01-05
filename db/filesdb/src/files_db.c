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
#include <zlib.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sstack_db.h>

#define DB_DIR "/var/tmp/.sfs_db"
#define NUM_DIRS 1024
#define FNAME_LEN 128

pthread_mutex_t files_db_mutex = PTHREAD_MUTEX_INITIALIZER;

int
hashfn(uint64_t key)
{
	return (key % NUM_DIRS);
}

// db_open, db_close are NOP for this DB

int
files_db_open(void)
{
	return 0;
}

int
files_db_close(void)
{
	return 0;
}

// db_init is where directory structure for the db is/are created
//

int
files_db_init(void)
{
	int ret = -1;
	int i;
	char dirname[FNAME_LEN];

	ret = mkdir(DB_DIR, 0755);
	if (ret != 0) {
		perror("mkdir: ");
		goto out;
	}
	// Create NUM_DIRS under DB_DIR
	for (i = 0; i < NUM_DIRS; i++) {
		sprintf(dirname, "%s/%d", DB_DIR, i);
		ret = mkdir(dirname, 0755);
		if (ret != 0) {
			perror("mkdir: ");
			goto out;
		}
	}
out:
	return ret;
}

// Meat of the DB
int
files_db_insert(uint64_t key, char *data, size_t len, db_type_t type)
{
	char filename[FNAME_LEN];
	gzFile outfile;
	int dir = hashfn(key);
	int ret = -1;

	pthread_mutex_lock(&files_db_mutex);
	sprintf(filename, "%s/%d/%"PRIu64".gz", DB_DIR, dir, key);
	outfile = gzopen( filename, "wb");
	if (NULL == outfile) {
		syslog(LOG_ERR, "%s: gzopen failed for  %s error %d\n",
			__FUNCTION__, filename, errno);
		pthread_mutex_unlock(&files_db_mutex);
		return -errno;
	}
	USYSLOG(LOG_ERR, "%s: Created file %s\n", __FUNCTION__, filename);
	ret = gzwrite(outfile, data, len);
	if (ret == 0) {
		syslog(LOG_ERR, "%s: gzwrite failed for %s error %d \n",
			__FUNCTION__, filename, errno);
		gzclose(outfile);
		pthread_mutex_unlock(&files_db_mutex);
		return -errno;
	} else if (ret != len) {
		syslog(LOG_ERR, "%s: Partial write done for %s error %d\n",
			__FUNCTION__, filename, errno);
		gzclose(outfile);
		pthread_mutex_unlock(&files_db_mutex);
		return -errno;
	}

	gzclose(outfile);
	pthread_mutex_unlock(&files_db_mutex);

	return 1;
}

// Seek to 'offset' from 'whence' and read data
// Uses gzseek and gzread
// Needed to read extents from inode
int
files_db_seekread(uint64_t key, char *data, size_t len, off_t offset,
	int whence, db_type_t type)
{
	char filename[FNAME_LEN];
	gzFile infile;
	int dir = hashfn(key);
	z_off_t off;
	size_t len1 = 0;

	pthread_mutex_lock(&files_db_mutex);
	sprintf(filename, "%s/%d/%"PRIu64".gz", DB_DIR, dir, key);
	infile = gzopen(filename, "rb");
	if (NULL == infile) {
		syslog(LOG_ERR, "%s: gzopen failed for %s error %d\n",
			__FUNCTION__, filename, errno);
		pthread_mutex_unlock(&files_db_mutex);
		return -errno;
	}
	off = gzseek(infile, offset, whence);
	if (off == -1) {
		syslog(LOG_ERR, "%s: gzseek failed for %s with error %d offset %d\n",
			__FUNCTION__, filename, errno, (int) offset);
		gzclose(infile);
		pthread_mutex_unlock(&files_db_mutex);
		return -errno;
	}
	len1 = gzread(infile, data, len);
	if (len1 == -1) {
		syslog(LOG_ERR, "%s: gzread failed for %s error %d \n",
			__FUNCTION__, filename, errno);
		gzclose(infile);
		pthread_mutex_unlock(&files_db_mutex);
		return -errno;
	} else if (len != len1) {
		syslog(LOG_ERR, "%s: Partial read done for %s error %d"
			"read len = %d requested len = %d \n",
			__FUNCTION__, filename, errno, (int) len1, (int)len);
		gzclose(infile);
		pthread_mutex_unlock(&files_db_mutex);
		return -errno;
	}
	gzclose(infile);
	pthread_mutex_unlock(&files_db_mutex);

	return len1;
}


int
files_db_update(uint64_t key, char *data, size_t len, db_type_t type)
{
	char filename[FNAME_LEN];
	gzFile outfile;
	int dir = hashfn(key);
	int ret = -1;
	off_t off = 0;

	pthread_mutex_lock(&files_db_mutex);
	sprintf(filename, "%s/%d/%"PRIu64".gz", DB_DIR, dir, key);
	// Get rid of the file first
	// Anyway it gets recreated.
	//unlink(filename);
	//sync();
	outfile = gzopen(filename, "wb");
	if (NULL == outfile) {
		syslog(LOG_ERR, "%s: gzopen failed for %s error %d\n",
			__FUNCTION__, filename, errno);
		pthread_mutex_unlock(&files_db_mutex);
		return -errno;
	}
	off = gzseek(outfile, 0, SEEK_SET);
	if (off == -1) {
		syslog(LOG_ERR, "%s: gzseek failed for %s with error %d offset %d\n",
			__FUNCTION__, filename, errno, 0);
		gzclose(outfile);
		pthread_mutex_unlock(&files_db_mutex);
		return -errno;
	}
	ret = gzwrite(outfile, data, len);
	if (ret == 0) {
		syslog(LOG_ERR, "%s: gzwrite failed for %s error %d \n",
			__FUNCTION__, filename, errno);
		gzclose(outfile);
		pthread_mutex_unlock(&files_db_mutex);
		return -errno;
	} else if (ret != len) {
		syslog(LOG_ERR, "%s: gzwrite is partial for %s error %d"
			"written len = %d requested len = %d\n",
			__FUNCTION__, filename, errno, ret, (int) len);
		gzclose(outfile);
		pthread_mutex_unlock(&files_db_mutex);
		return -errno;
	}
	gzclose(outfile);
	pthread_mutex_unlock(&files_db_mutex);

	return 1;
}

int
files_db_get(uint64_t key, char *data, size_t len, db_type_t type)
{
	char filename[FNAME_LEN];
	gzFile infile;
	int dir = hashfn(key);
	size_t len1 = 0;

	pthread_mutex_lock(&files_db_mutex);
	sprintf(filename, "%s/%d/%"PRIu64".gz", DB_DIR, dir, key);
	infile = gzopen(filename, "rb");
	if (NULL == infile) {
		syslog(LOG_ERR, "%s: gzopen failed for %s error %d\n",
			__FUNCTION__, filename, errno);
		pthread_mutex_unlock(&files_db_mutex);
		return -errno;
	}
	len1 = gzread(infile, data, len);
	if (len1 == -1) {
		syslog(LOG_ERR, "%s: gzread failed for %s error %d\n", __FUNCTION__,
			filename, errno);
		gzclose(infile);
		pthread_mutex_unlock(&files_db_mutex);
		return -errno;
	} else if (len1 != len) {
		syslog(LOG_ERR, "%s: gzread is partial for %s error %d"
			"read len = %d requested len = %d\n",
			__FUNCTION__, filename, errno, (int) len1, (int) len);
		gzclose(infile);
		pthread_mutex_unlock(&files_db_mutex);
		return -errno;
	}
	gzclose(infile);
	pthread_mutex_unlock(&files_db_mutex);

	return len1;
}

int
files_db_delete(uint64_t key)
{
	char filename[FNAME_LEN];
	int dir = hashfn(key);
	int ret_status = -1;

	pthread_mutex_lock(&files_db_mutex);
	sprintf(filename, "%s/%d/%"PRIu64".gz", DB_DIR, dir, key);
	ret_status = unlink(filename);
	if (ret_status != 0) {
		syslog(LOG_ERR, "%s: Failed to delete %s error %d\n", __FUNCTION__,
			filename, errno);
		pthread_mutex_unlock(&files_db_mutex);
		return -errno;
	}

	pthread_mutex_unlock(&files_db_mutex);

	return 1;
}

int
files_db_cleanup(void)
{
	char command[PATH_MAX + 8];
	int ret_status = -1;

	pthread_mutex_lock(&files_db_mutex);
	sprintf(command, "rm -rf %s", DB_DIR);
	ret_status = system(command);
	if (ret_status == -1)  {
		USYSLOG(LOG_ERR, "%s: Shell command is incomplete %s\n",
				__FUNCTION__, command);
		pthread_mutex_unlock(&files_db_mutex);
	} else {
		USYSLOG(LOG_INFO, "%s: system() succeeded with return status %d\n",
			__FUNCTION__, WEXITSTATUS(ret_status));
		pthread_mutex_unlock(&files_db_mutex);
	}

	pthread_mutex_unlock(&files_db_mutex);
	return ret_status;
}
