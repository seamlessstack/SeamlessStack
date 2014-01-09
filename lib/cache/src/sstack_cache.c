/*************************************************************************
 *
 * SEAMLESSSTACK CONFIDENTIAL
 * __________________________
 *
 *  [2012] - [2013]  SeamlessStack Inc
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

#include <inttypes.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <dlfcn.h>
#include <errno.h>
#include <red_black_tree.h>
#include <sstack_log.h>
#include <sstack_jobs.h>
#include <sstack_helper.h>
#include <sstack_cache.h>
#include <sstack_cache_api.h>

#define SSD_CACHE_LIBNAME "/opt/sfs/lib/libssdcache.so"

rb_red_blk_tree *cache_tree = NULL;
pthread_spinlock_t cache_lock;

/*
 * compare_func - RB-tree compare function
 */
static int
compare_func(const void *val1, const void *val2)
{
	// NOTE:
	// Hashkeys are generated using SHA256. So it is unlikely that
	// hashkeys will clash.
	switch(strcmp((const char *) val1, (const char *) val2)) {
		case -1: return -1;
		case 1: return 1;
	}

	return 0;
}

/*
 * destroy_func - RB-tree node free function
 */
static void
destroy_func(void *val)
{
	if (val)
		free(val);
}


/*
 * cache_init - Create RB-tree to maintain cache metadata
 *
 * Returns initialized cache_tree pointer on success. Returns NULL upon
 * failure.
 */

rb_red_blk_tree *
cache_init(log_ctx_t *ctx)
{
	// Create RB-tree for managing the cache
	cache_tree = RBTreeCreate(compare_func, destroy_func, NULL, NULL, NULL);
	if (NULL == cache_tree)
		sfs_log(ctx, SFS_ERR, "%s: Failed to create RB-tree for cache \n",
						__FUNCTION__);

	pthread_spin_init(&cache_lock, PTHREAD_PROCESS_PRIVATE);

	return cache_tree;
}

/*
 * sstack_memcache_evict_objects - Create space in memcached by evicting
 * 									objects based on LRU.
 *
 * NOTE:
 * This is heart of the two-tier caching functionality. This function checks
 * for availability of SSD caching functionality before throwing out the
 * items permanently.
 * If SSD caching is enabled, evicted items are pushed to SSD. If SSD is full,
 * SSD caching functionality will evict items according to chosen algo.
 */

int
sstack_memcache_evict_objects(void)
{
	// FIXME:
	// Eviction functionality goes here

#if 0
	// Check whether SSD caching is enabled
	if (ssd_caching_enabled && ssd_cache && ssd_cache->ops.ssd_cache_init) {
		// Store the elements into cache
		if (ssd_cache && ssd_cache->ops.ssd_cache_store) {
			ssd_cache_entry_t entry;

			entry = ssd_cache->ops.ssd_cache_store(handle, data, len, ctx);
		}
	} else {
		// Do nothing as objects are already evicted.
	}
#endif
	return 0;
}

/*
 * sstack_cache_store - Store data into cache
 *
 * data - data to be stored. Should be non-NULL
 * len - Length of data to be stored. Should be >0
 * entry - cache structure. Should be non-NULL
 * ctx - log context
 *
 * Returns 0 on success and -1 on failure.
 */

int
sstack_cache_store(void *data, size_t len, sstack_cache_t *entry,
				log_ctx_t *ctx)
{
	int ret = -1;
	rb_red_blk_node *node = NULL;

	// Parameter validation
	if (NULL == entry || NULL == data || len <= 0) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);
		errno = EINVAL;

		return -1;
	}
	// Store the entry into cache
	ret = sstack_memcache_store(entry->memcache.mc,
					(const char *) entry->hashkey,
					(const char *) data, len, ctx);
	if (ret != 0) {
		sfs_log(ctx, SFS_ERR, "%s: sstack_cache_store failed \n",
						__FUNCTION__);
		if (ret == -MEMCACHED_MEMORY_ALLOCATION_FAILURE) {
			// Memcached is out of memory.
			// This is due to configuring memcached to not to evict objects
			// and fail stores with PROTOCOL_BINARY_RESPONSE_ENOMEM error
			// code.
			// FIXME:
			// libmemcached returns MEMCACHED_MEMORY_ALLOCATION_FAILURE
			// for this error. But it also seem to return same error code
			// when malloc fails internally. This needs to be fixed.
			sfs_log(ctx, SFS_INFO, "%s: Memcached is out of memory. "
							"Objects will be evicted based on LRU \n",
							__FUNCTION__);
			ret = sstack_memcache_evict_objects();
			// Retry store again
			ret = sstack_memcache_store(entry->memcache.mc,
					(const char *) entry->hashkey,
					(const char *) data, len, ctx);
			if (ret != 0) {
				// Failed to store even after making space
				// Log an error and exit
				sfs_log(ctx, SFS_ERR, "%s: Failed to store element in cache."
								"Error = %d \n", __FUNCTION__, -ret);

				return -1;
			}
		} else {
			// Some other error
			return -1;
		}
	}
	// memcache store succeeded.
	entry->on_ssd = false;
	entry->time = time(NULL);

	pthread_spin_lock(&cache_lock);
	node = RBTreeInsert(cache_tree, (void *) entry->hashkey, (void *) entry);
	if (NULL == node) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to insert node into RB-tree \n",
						__FUNCTION__);
		sstack_cache_remove(entry->memcache.mc,
						(const char *) entry->hashkey, ctx);
		pthread_spin_unlock(&cache_lock);

		return -1;
	}
	pthread_spin_unlock(&cache_lock);

	return 0;
}

/*
 * sstack_cache_search - Search for cached entry
 *
 * hashkey - Hash of the key to be searched. Should be non-NULL
 * ctx - log context
 *
 * Returns the sstack_cache_t pointer on success and NULL on failure.
 */

sstack_cache_t *
sstack_cache_search(uint8_t *hashkey, log_ctx_t *ctx)
{
	sstack_cache_t c;
	// sstack_cache_t *ret = NULL;
	rb_red_blk_node *node = NULL;

	// Parameter validation
	if (NULL == hashkey) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid paramter specified \n",
						__FUNCTION__);
		errno = EINVAL;

		return NULL;
	}

	memcpy((void *) &c.hashkey, (void *) hashkey, (SHA256_DIGEST_LENGTH + 1));

	pthread_spin_lock(&cache_lock);
	node = RBExactQuery(cache_tree, (void *) &c);
	if (NULL == node) {
		sfs_log(ctx, SFS_CRIT, "%s: hashkey %s not found in RB-tree \n",
						__FUNCTION__, (char *) hashkey);
		errno = EFAULT;
		pthread_spin_unlock(&cache_lock);

		return NULL;
	}
	pthread_spin_unlock(&cache_lock);

	return node->info;
}

/*
 * sstack_cache_purge - Purge the entry corresponding to hashkey from
 *						memcached
 *
 * hashkey - Key
 * ctx - Log context
 *
 * Returns 0 on success and -1 on failure.
 */

int
sstack_cache_purge(uint8_t *hashkey, log_ctx_t *ctx)
{
	sstack_cache_t c;
	sstack_cache_t *entry = NULL;
	rb_red_blk_node *node = NULL;

	// Parameter validation
	if (NULL == hashkey) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid parameter specified \n",
						__FUNCTION__);
		errno = EINVAL;

		return -1;
	}

	memcpy((void *) &c.hashkey, (void *) hashkey, (SHA256_DIGEST_LENGTH + 1));

	pthread_spin_lock(&cache_lock);
	node = RBExactQuery(cache_tree, (void *) &c);
	if (NULL == node) {
		sfs_log(ctx, SFS_CRIT, "%s: hashkey %s not found in RB-tree \n",
						__FUNCTION__, (char *) hashkey);
		errno = EFAULT;
		pthread_spin_unlock(&cache_lock);

		return -1;
	}

	// Delete the node from the tree and from memcached pool
	entry = (sstack_cache_t *) node->info;
	sstack_cache_remove(entry->memcache.mc,
					(const char *) entry->hashkey, ctx);
	RBDelete(cache_tree, node);
	pthread_spin_unlock(&cache_lock);

	return 0;
}

/*
 * ssd_cache_register - Register ssd caching infrastructure
 */

ssd_cache_t *
ssd_cache_register(log_ctx_t *ctx)
{
	ssd_cache_t *cache = NULL;
	void *handle = NULL;

	cache = create_ssd_cache();
	if (NULL == cache) {
		sfs_log(ctx, SFS_ERR, "%s: create_ssd_cache failed. Out of memory \n",
						__FUNCTION__);

		return NULL;
	}

	handle = dlopen(SSD_CACHE_LIBNAME, RTLD_NOW);
	if (NULL == handle) {
		sfs_log(ctx, SFS_ERR, "%s: dlopen failed on %s. Error = %d \n",
						__FUNCTION__, errno);

		free(cache);
		return NULL;
	}

	cache->ops.ssd_cache_init = (ssd_cache_init_t)
			dlsym(handle, "sstack_ssd_cache_init");
	if (NULL == cache->ops.ssd_cache_init) {
		sfs_log(ctx, SFS_ERR, "%s: sstack_ssd_cache_init not defined \n",
						__FUNCTION__);
		errno = EINVAL;
		free(cache);

		return NULL;
	}

	cache->ops.ssd_cache_open = (ssd_cache_open_t)
			dlsym(handle, "sstack_ssd_cache_open");
	if (NULL == cache->ops.ssd_cache_open) {
		sfs_log(ctx, SFS_ERR, "%s: sstack_ssd_cache_open not defined \n",
						__FUNCTION__);
		errno = EINVAL;
		free(cache);

		return NULL;
	}

	cache->ops.ssd_cache_close = (ssd_cache_close_t)
			dlsym(handle, "sstack_ssd_cache_close");
	if (NULL == cache->ops.ssd_cache_close) {
		sfs_log(ctx, SFS_ERR, "%s: sstack_ssd_cache_close not defined \n",
						__FUNCTION__);
		errno = EINVAL;
		free(cache);

		return NULL;
	}

	cache->ops.ssd_cache_store = (ssd_cache_store_t)
			dlsym(handle, "sstack_ssd_cache_store");
	if (NULL == cache->ops.ssd_cache_store) {
		sfs_log(ctx, SFS_ERR, "%s: sstack_ssd_cache_store not defined \n",
						__FUNCTION__);
		errno = EINVAL;
		free(cache);

		return NULL;
	}

	cache->ops.ssd_cache_purge = (ssd_cache_purge_t)
			dlsym(handle, "sstack_ssd_cache_purge");
	if (NULL == cache->ops.ssd_cache_purge) {
		sfs_log(ctx, SFS_ERR, "%s: sstack_ssd_cache_purge not defined \n",
						__FUNCTION__);
		errno = EINVAL;
		free(cache);

		return NULL;
	}

	cache->ops.ssd_cache_update = (ssd_cache_update_t)
			dlsym(handle, "sstack_ssd_cache_update");
	if (NULL == cache->ops.ssd_cache_update) {
		sfs_log(ctx, SFS_ERR, "%s: sstack_ssd_cache_update not defined \n",
						__FUNCTION__);
		errno = EINVAL;
		free(cache);

		return NULL;
	}

	cache->ops.ssd_cache_retrieve = (ssd_cache_retrieve_t)
			dlsym(handle, "sstack_ssd_cache_retrieve");
	if (NULL == cache->ops.ssd_cache_retrieve) {
		sfs_log(ctx, SFS_ERR, "%s: sstack_ssd_cache_retrieve not defined \n",
						__FUNCTION__);
		errno = EINVAL;
		free(cache);

		return NULL;
	}

	cache->ops.ssd_cache_destroy = (ssd_cache_destroy_t)
			dlsym(handle, "sstack_ssd_cache_destroy");
	if (NULL == cache->ops.ssd_cache_destroy) {
		sfs_log(ctx, SFS_ERR, "%s: sstack_ssd_cache_destroy not defined \n",
						__FUNCTION__);
		errno = EINVAL;
		free(cache);

		return NULL;
	}

	dlclose(handle);

	return cache;
}

