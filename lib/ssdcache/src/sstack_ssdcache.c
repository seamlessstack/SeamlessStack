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

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/statvfs.h>
#include <sstack_log.h>
#include <sstack_ssdcache.h>

#define COMMAND_LEN 1024

// BSS

// Array to maintain individual SSD cache strcutures
ssd_cache_struct_t ssd_caches[MAX_SSD_CACHEDEV];
// Lock for manipulating ssd_caches
pthread_spinlock_t cache_list_lock;
// Indicates the number of SSD caches enabled in the system
ssd_cache_handle_t cur_ssd_cache_handle = 0;

/*
 * ssd_create_cachedev - create file system
 *
 * path -
 */

static ssd_cachedev_info_t *
ssd_create_cachedev(char *path, int64_t size, log_ctx_t *ctx)
{
	ssd_cachedev_info_t *cachedev = NULL;
	int ret = -1;
	pid_t pid;
	char command[COMMAND_LEN] = { '\0' };
	int status;
	char *mntpnt_name = NULL;
	struct statvfs vfs;

	if (NULL == path || size < 0)
		return NULL;

	// NOTE:
	// This command is tuned for cache file size of 16KiB (32 blocks)
	// and journal size of 64 MiB
	sprintf(command, "/sbin/mkfs.ext4 -i 32 -J size=64 %s %"PRId64"",
					path, size);
	// Create file system on the device

	ret = system(command);
	if (ret == -1) {
		sfs_log(ctx, SFS_ERR, "%s: execve failed with error %d \n",
						__FUNCTION__, errno);

		return NULL;
	}

	// mkfs succeeded.

	mntpnt_name = mkdtemp("/tmp/SSDXXXXXX");
	if (NULL == mntpnt_name) {
		sfs_log(ctx, SFS_ERR, "%s: mkdtemp failed with errno %d \n",
						__FUNCTION__, errno);

		return NULL;
	}

	// Mount the file system on SSD on mntpnt_name
	sprintf(command, "mount -t ext4 %s %s", path, mntpnt_name);
	ret = system(command);
	if (ret == -1) {
		sfs_log(ctx, SFS_ERR, "%s: mount of %s on %s failed. Error = %d\n",
							__FUNCTION__, path, mntpnt_name, errno);

		return NULL;
	}
	cachedev = (ssd_cachedev_info_t *) calloc(sizeof(ssd_cachedev_info_t),
					1);
	if (NULL == cachedev) {
		sfs_log(ctx, SFS_ERR, "%s: Memory allocation failed \n",
							__FUNCTION__);

		return NULL;
	}

	strcpy(cachedev->mntpnt, mntpnt_name);
	ret = statvfs(mntpnt_name, &vfs);
	// Assume statvfs succeeded
	cachedev->num_cachelines = (uint64_t) vfs.f_files;

	return cachedev;
}


// SSD entry points

/*
 * sstack_ssd_cache_init - Initialize data structures for the SSD cache dev
 *
 * path - Device path of the SSD cache. Should be non-NULL.
 * size - Size in number KBs. Should be > 0
 * ctx - log context
 *
 * - Gets the next cache handle
 * - mkfs.ext4 on the cache device and mounts it on a local mount point
 * - Populates ssd_caches array
 *
 * Returns 0 on success and -1 on failure.
 * Note:
 * This routine is called by ssrack_add_cache which is called with
 * root privilege.
 *
 */

ssd_cache_handle_t
sstack_ssd_cache_init(char *path, int64_t size, log_ctx_t *ctx)
{
	ssd_cache_handle_t handle;
	ssd_cache_struct_t cache_struct;
	ssd_cachedev_info_t *info = NULL;

	// Parameter validation
	if (NULL == path || size < 0) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid parameter specified \n",
						__FUNCTION__);
		errno = EINVAL;

		return -1;
	}

	// NOTE:
	// Populate local data structure and copy this to ssd_caches.
	// This is to use pthread spinlock instead of mutex and limit
	// critical section

	cache_struct.handle = handle;
	info = ssd_create_cachedev(path, size, ctx);
	if (NULL == info) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to create cachedev. Error = %d \n",
						__FUNCTION__, errno);

		return -1;
	}

	cache_struct.stats.inuse = 0;
	// Get number of inodes in the file system
	cache_struct.stats.num_cachelines = info->num_cachelines;
	strncpy((char *) &cache_struct.mountpt, info->mntpnt, PATH_MAX);
	// Get cache handle
	handle = get_next_ssd_cache_handle();
	if (handle == -1) {
		sfs_log(ctx, SFS_ERR, "%s: get_next_ssd_cache_handle returned error. "
						"Either max cache devices are under use or fatal "
						"error.\n", __FUNCTION__);
		errno = ENOENT;

		return -1;
	}
	pthread_spin_lock(&cache_list_lock);
	memcpy((void *) &ssd_caches[handle], &cache_struct,
					sizeof(ssd_cache_struct_t));
	pthread_spin_init(&ssd_caches[handle].stats.lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_unlock(&cache_list_lock);

	return 0;
}

/*
 * sstack_ssd_cache_open - SSD cache open entry point
 *
 * handle - SSD cache handle
 * ctx - log context
 *
 * Returns 0 on success and -1 on failure.
 * NOTE:
 * This is just a placeholder for now.
 */

int
sstack_ssd_cache_open(ssd_cache_handle_t handle, log_ctx_t *ctx)
{
	// Parameter validation
	if (handle < 0) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid parameter specified \n",
						__FUNCTION__);
		errno = EINVAL;

		return -1;
	}
	sfs_log(ctx, SFS_INFO, "%s: SSD cache with handle %d opened "
					"successfully\n", __FUNCTION__, handle);

	return 0;
}

/*
 * sstack_ssd_cache_close - SSD cache close entry point
 *
 * handle - SSD cache handle
 * ctx - log context
 *
 * NOTE:
 * This is just a placeholder for now.
 */

void
sstack_ssd_cache_close(ssd_cache_handle_t handle, log_ctx_t *ctx)
{
	// Parameter validation
	if (handle < 0) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid parameter specified \n",
						__FUNCTION__);
		errno = EINVAL;

		return;
	}
	sfs_log(ctx, SFS_INFO, "%s: SSD cache with handle %d closed "
					"successfully\n", __FUNCTION__, handle);

}

