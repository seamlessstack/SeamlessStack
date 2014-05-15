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
#include <stdlib.h>
#include <sys/statvfs.h>
#include <sstack_log.h>
#include <sstack_ssdcache.h>
#include <sstack_ssdcache_files.h>
#include <sstack_bitops.h>

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
	// This command is tuned for cache file size of 64KiB (128 blocks)
	// and journal size of 64 MiB
	sprintf(command, "/sbin/mkfs.ext4 -i 128 -J size=64 %s %"PRId64"",
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

/* compare and destroy functions for md tree */
static int
md_comp_func(const void *val1, const void *val2)
{
	if (*(ssd_cache_entry_t *)val1 > *(ssd_cache_entry_t *)val2)
		return 1;
	if (*(ssd_cache_entry_t *)val1 < *(ssd_cache_entry_t *)val2)
		return -1;

	return 0;
}

static void
md_destroy_func(void *val)
{
	if (val)
		free(val);

}

/* compare and destroy functions for LRU tree */
static int
lru_comp_func(const void *val1, const void *val2)
{
	if (*(time_t *)val1 > *(time_t *)val2)
		return 1;
	if (*(time_t *)val1 < *(time_t *)val2)
		return -1;

	return 0;
}

static void
lru_destroy_func(void *val)
{
	if (val)
		free(val);

}

/*
 * ssdmd_add - Add the entry into md_tree and update lru tree
 *
 * cache_struct - ssd cache information structure. Should be non-NULL.
 * md_entry - metadata entry. Should be non-NULL.
 * ctx - Log context
 *
 * Adds an entry into md_tree and lru_tree.
 * Returns 0 on success and -1 on failure.
 */

static int
ssdmd_add(ssd_cache_struct_t *cache_struct, ssdcachemd_entry_t *md_entry,
				log_ctx_t *ctx)
{
	ssdcachelru_entry_t *lru_entry = NULL;
	rb_red_blk_node *node = NULL;
	time_t t;

	// Parameter validation
	if (NULL == cache_struct || NULL == md_entry) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);
		errno = EINVAL;

		return -1;
	}

	// Insert into md_tree
	pthread_spin_lock(&cache_struct->md_lock);
	node = RBTreeInsert(cache_struct->md_tree,
					(void *) md_entry->ssd_ce, (void *) md_entry);
	if (NULL == node) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to insert mdentry for ce"
						" %d\n", __FUNCTION__, md_entry->ssd_ce);
		pthread_spin_unlock(&cache_struct->md_lock);

		return -1;
	}
	pthread_spin_lock(&cache_struct->md_lock);

	// Insert into lru_tree
	lru_entry = calloc(sizeof(ssdcachelru_entry_t), 1);
	if (NULL == lru_entry) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to allocate memory for lruentry\n",
						__FUNCTION__);
		RBDelete(cache_struct->md_tree, node);

		return -1;
	}

	t = time(NULL);
	pthread_spin_lock(&cache_struct->lru_lock);
	node = RBTreeInsert(cache_struct->lru_tree,
					(void*) t, (void *) lru_entry);
	if (NULL == node) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to insert lruentry for ce"
						" %d\n", __FUNCTION__, md_entry->ssd_ce);
		pthread_spin_unlock(&cache_struct->lru_lock);
		free(lru_entry);
		RBDelete(cache_struct->md_tree, node);

		return -1;
	}
	pthread_spin_unlock(&cache_struct->lru_lock);

	return 0;
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
	int ret = -1;

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
	// Create ssdmd tree and LRU tree for the SSD cache device
	cache_struct.md_tree = RBTreeCreate(md_comp_func, md_destroy_func, NULL,
					                     NULL, NULL);
	if (NULL == cache_struct.md_tree) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to allocate memory for md_tree \n",
						__FUNCTION__);
		// FIXME:
		// Implement and call free_ssd_cache_handle(handle);
		return -1;
	}
	pthread_spin_init(&cache_struct.md_lock, PTHREAD_PROCESS_PRIVATE);

	cache_struct.lru_tree = RBTreeCreate(lru_comp_func, lru_destroy_func, NULL,
					NULL, NULL);
	if (NULL == cache_struct.lru_tree) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to allocate memory for lru_tree \n",
						__FUNCTION__);
		RBTreeDestroy(cache_struct.md_tree);
		pthread_spin_destroy(&cache_struct.md_lock);
		// FIXME:
		// Implement and call free_ssd_cache_handle(handle);
		return -1;
	}
	pthread_spin_init(&cache_struct.lru_lock, PTHREAD_PROCESS_PRIVATE);

	// Create ce_bitmap
	cache_struct.ce_bitmap = sfs_init_bitmap(
					cache_struct.stats.num_cachelines, ctx);
	if (ret == -1) {
		RBTreeDestroy(cache_struct.md_tree);
		pthread_spin_destroy(&cache_struct.md_lock);
		pthread_spin_destroy(&cache_struct.lru_lock);
		// FIXME:
		// Implement and call free_ssd_cache_handle(handle);
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

/*
 * get_ssdcache_entry - Obtain free cache entry in the given SSD cache
 *
 * cache_struct - Structure representing unique SSD cache device. Shoule be
 *					 non-NULL
 *
 * Returns next avaialable entry on success and returns -1 on failure.
 */

static ssd_cache_entry_t
get_ssdcache_entry(ssd_cache_struct_t *cache_struct, log_ctx_t *ctx)
{
	int max_entries = -1;
	ssd_cache_entry_t ssd_ce = -1;

	// Parameter validation
	if (NULL == cache_struct) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid parameter specified \n",
						__FUNCTION__);
		errno = EINVAL;

		return -1;
	}
	// Get SSD cache device specific maximum cache lines
	pthread_spin_lock(&cache_struct->stats.lock);
	max_entries = cache_struct->stats.num_cachelines;
	pthread_spin_unlock(&cache_struct->stats.lock);

	pthread_mutex_lock(&cache_struct->ssd_ce_mutex);
	if (cache_struct->current_ssd_ce >= max_entries) {
		int i = 0;

		// Wrap around happened
		// Set cache_struct->current_ssd_ce to the first undet bit in the bitmap
		for (i = 0; i < max_entries; i++) {
			if (BITTEST(cache_struct->ce_bitmap, i) == 0)
				break;
		}
		if (i == max_entries) {
			sfs_log(ctx, SFS_ERR, "%s: All SSD cache entry slots currently "
							"occupied.\n", __FUNCTION__);
			// FIXME:
			// Use LRU to manage cache
			return -1;
		}
		ssd_ce = i;
		cache_struct->current_ssd_ce = i;
		BITSET(cache_struct->ce_bitmap,i); // Make this bit busy
		// Reinitalize cache_struct->current_ssd_ce
		for (i = cache_struct->current_ssd_ce; i < max_entries; i++) {
			if (BITTEST(cache_struct->ce_bitmap, i) == 0)
				break;
		}
		if (i < max_entries) {
			cache_struct->current_ssd_ce = i;
		} else {
			// No free slots available at this time for SSD cache entry
			// Set current ssd cache entry in such a way that next caller
			// will have to go through bitmap once again.
			cache_struct->current_ssd_ce = max_entries;
		}
	} else {
		// No wrap around
		ssd_ce = cache_struct->current_ssd_ce;
		cache_struct->current_ssd_ce ++;
		BITSET(cache_struct->ce_bitmap,ssd_ce); // Make this bit busy
	}

	pthread_mutex_unlock(&cache_struct->ssd_ce_mutex);
	// Update cache stats
	pthread_spin_lock(&cache_struct->stats.lock);
	cache_struct->stats.inuse ++;
	pthread_spin_unlock(&cache_struct->stats.lock);

	return ssd_ce;
}

/*
 * free_ssdcache_entry - Free SSD cache entry
 *
 * cache_struct - Structure representing unique SSD cache device
 * entry - unique id representing cache line
 * ctx - Log context
 *
 * Frees the cache entry if valid.
 */

static int
free_ssdcache_entry(ssd_cache_struct_t *cache_struct, ssd_cache_entry_t entry,
				log_ctx_t *ctx)
{
	// Validate
	if (NULL == cache_struct) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid parameter specified\n",
						__FUNCTION__);
		errno = EINVAL;
		return -1;
	}

	pthread_spin_lock(&cache_struct->stats.lock);
	if (entry > cache_struct->stats.num_cachelines) {
		pthread_spin_unlock(&cache_struct->stats.lock);
		sfs_log(ctx, SFS_ERR, "%s: Invalid parameter specified\n",
						__FUNCTION__);
		errno = EINVAL;
		return -1;
	}
	pthread_spin_unlock(&cache_struct->stats.lock);

	pthread_mutex_lock(&cache_struct->ssd_ce_mutex);
	BITCLEAR(cache_struct->ce_bitmap, entry);
	pthread_mutex_unlock(&cache_struct->ssd_ce_mutex);

	return 0;
}

/*
 * sstack_ssd_cache_store - Store data into SSD cache
 *
 * handle - unique id representing SSD. Should be >0
 * data - Data to be stored. Should be non-NULL
 * size - Size of the data. Shoule be >0
 * ctx - Log context
 *
 * Stores the data into SSD cache and returns entry corresponding to the data.
 *
 * Returns non-zero cache entry on success and returns -1 on failure.
 */

ssd_cache_entry_t
sstack_ssd_cache_store(ssd_cache_handle_t handle, void *data,
				size_t size, log_ctx_t *ctx)
{
	ssd_cache_entry_t ssd_ce = -1;
	ssd_cache_struct_t *cache_struct = NULL;
	ssdcachemd_entry_t *md_entry = NULL;
	char filename[FILENAME_MAX + MOUNT_PATH_MAX] = { '\0' };
	int fd = -1;
	int ret = -1;

	// Parameter validation
	if (handle < 0 || NULL == data || size <= 0) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid parameters passed \n", __FUNCTION__);
		errno = EINVAL;

		return -1;
	}

	pthread_spin_lock(&cache_list_lock);
	if (handle > cur_ssd_cache_handle) {
		pthread_spin_unlock(&cache_list_lock);
		sfs_log(ctx, SFS_ERR, "%s: Invalid SSD cache handle specified. "
						"Handle = %d Current handle = %d\n", __FUNCTION__,
						handle, cur_ssd_cache_handle);
		errno = EINVAL;

		return -1;
	}
	cache_struct = &ssd_caches[handle];
	pthread_spin_unlock(&cache_list_lock);

	ssd_ce = get_ssdcache_entry(cache_struct, ctx);
	if (ssd_ce < 0) {
		sfs_log(ctx, SFS_ERR, "%s: get_ssdcache_entry failed \n", __FUNCTION__);
		errno = EIO;

		return -1;
	}

	// Create a new file name
	sprintf(filename, "%s/cacheXXXXXX", cache_struct->mountpt);
	fd = mkstemp(filename);
	if (fd == -1) {
		sfs_log(ctx, SFS_ERR, "%s: mkstemp failed with error %d \n",
						__FUNCTION__, errno);
		free_ssdcache_entry(cache_struct, ssd_ce, ctx);

		return -1;
	}
	// Write the data into the file
	ret = write(fd, data, size);
	if (ret < size) {
		unlink(filename);
		sfs_log(ctx, SFS_ERR, "%s: Failed to write data to SSD cache. "
						"Error = %d \n", __FUNCTION__, errno);
		free_ssdcache_entry(cache_struct, ssd_ce, ctx);

		return -1;
	}
	close(fd);

	// Add cache entry to filename mapping
	md_entry = malloc(sizeof(ssdcachemd_entry_t));
	if (NULL == md_entry) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to allocate memory for ssd "
						"cache metadata entry\n", __FUNCTION__);
		unlink(filename);
		free_ssdcache_entry(cache_struct, ssd_ce, ctx);

		return -1;
	}

	md_entry->ssd_ce = ssd_ce;
	strcpy(md_entry->name, filename);
	// Insert metadata entry into RB tree
	ret = ssdmd_add(cache_struct, md_entry, ctx);
	if (ret == -1) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to insert ssd cache metadata into "
						"mdstore \n", __FUNCTION__);
		unlink(filename);
		free_ssdcache_entry(cache_struct, ssd_ce, ctx);
		free(md_entry);

		return -1;
	}

	return ssd_ce;
}


int
sstack_ssd_cache_purge(ssd_cache_handle_t handle, ssd_cache_entry_t entry,
				log_ctx_t * ctx)
{
}


int
sstack_ssd_cache_update(ssd_cache_handle_t handle, ssd_cache_entry_t entry, void
				*data, size_t size, log_ctx_t *ctx)
{

}

void *
sstack_ssd_cache_retrieve(ssd_cache_handle_t handle, ssd_cache_entry_t entry,
				size_t size, log_ctx_t *ctx)
{
}

void
sstack_ssd_cache_destroy(ssd_cache_handle_t handle)
{
}

ssd_cache_handle_t
sstack_ssd_cache_get_handle(void) 
{
}
