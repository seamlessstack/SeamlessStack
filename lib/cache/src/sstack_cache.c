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
#include <sstack_lrucache.h>
#include <sstack_cache_api.h>
#include <openssl/sha.h>

//#define sfs_log(a,b,c,...) printf(c, ##__VA_ARGS__)
#define SSD_CACHE_LIBNAME "/opt/sfs/libssdcache.so"
#define NUM_EVICT 8

rb_red_blk_tree *cache_tree = NULL;
pthread_spinlock_t cache_lock;
ssd_cache_t *ssd_cache = NULL;

/*
 * compare_func - RB-tree compare function
 */
static int
compare_func(const void *val1, const void *val2)
{
	// NOTE:
	// Hashkeys are generated using SHA256. So it is unlikely that
	// hashkeys will clash.
	switch(memcmp((const char *) val1, (const char *) val2,
			SHA256_DIGEST_LENGTH)) {
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

static void
destroy_key(void *val)
{
	/* Do nothing. Place holder to avoid RB tree crash */
}

/*
 * cache_init - Create RB-tree to maintain cache metadata
 *
 * Returns 0 on success and -1 on failure.
 */

int
cache_init(log_ctx_t *ctx)
{
	// Create RB-tree for managing the cache
	cache_tree = RBTreeCreate(compare_func, destroy_key, destroy_func,
								NULL, NULL);
	if (NULL == cache_tree) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to create RB-tree for cache \n",
						__FUNCTION__);
		return -1;
	}

	pthread_spin_init(&cache_lock, PTHREAD_PROCESS_PRIVATE);

	lru_tree = lru_init(ctx);
	if (NULL == lru_tree) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to create RB-tree for LRU \n",
						__FUNCTION__);
		return -1;
	}

	ssd_cache = ssd_cache_register(ctx);
	if (ssd_cache == NULL) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to register ssd_cache\n",
						__FUNCTION__);
		return (-1);
	}

	return 0;
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
sstack_memcache_evict_objects(log_ctx_t *ctx)
{
	int ret = -1;

	// Evict NUM_EVICT entries each time
	// FIXME: Set NUM_EVICT for performance
	ret = lru_demote_entries(lru_tree, NUM_EVICT, ctx);
	if (ret == -1) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to demote cache entries \n",
						__FUNCTION__);

		return -1;
	}

	return 0;
}

sstack_cache_t *
sstack_allocate_cache_entry(log_ctx_t *ctx)
{
	sstack_cache_t *cache_entry = NULL;

	cache_entry = (sstack_cache_t *) calloc(sizeof(sstack_cache_t), 1);
	if (NULL == cache_entry) {
		sfs_log(ctx, SFS_ERR, "%s: Out of memory \n", __FUNCTION__);

		return NULL;
	}
	pthread_mutex_init(&cache_entry->lock, NULL);
	cache_entry->on_ssd = false;

	return cache_entry;
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
			ret = sstack_memcache_evict_objects(ctx);
			if (ret == -1) {
				// Unable to create space in memcached cache
				// Fail the request
				sfs_log(ctx, SFS_ERR, "%s: sstack_memcache_evict_objects "
								"failed\n", __FUNCTION__);

				return -1;
			}
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
		sfs_log(ctx, SFS_ERR, "%s: Failed to insert node into cache tree "
						". Hash = %s \n", __FUNCTION__, entry->hashkey);

		return -1;
	}
	pthread_spin_unlock(&cache_lock);
	// Insert into LRU tree
	ret = lru_insert_entry(lru_tree, (void *) entry->hashkey, ctx);
	if (ret == -1) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to insert node into LRU tree. "
						"Hash = %s\n", __FUNCTION__, entry->hashkey);
		sstack_memcache_remove(entry->memcache.mc,
						(const char *) entry->hashkey, ctx);
		pthread_spin_lock(&cache_lock);
		RBDelete(cache_tree, node);
		pthread_spin_unlock(&cache_lock);

		return -1;
	}

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

	//memcpy((void *) &c.hashkey, (void *) hashkey, (SHA256_DIGEST_LENGTH + 1));

	pthread_spin_lock(&cache_lock);
	node = RBExactQuery(cache_tree, (void *) hashkey);
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
 * sstack_cache_get - Get cached data
 *
 * hashkey - Hash of the key of the data. Should be non-NULL
 * ctx - log context
 * size - number of bytes to read from cache
 *
 * Returns the data on success and NULL on failure. On partial data,
 * returns NULL.
 */

char *
sstack_cache_get(uint8_t *hashkey, size_t size, log_ctx_t *ctx)
{
	sstack_cache_t *c;
	// sstack_cache_t *ret = NULL;
	rb_red_blk_node *node = NULL;
	char *data = NULL;

	// Parameter validation
	if (NULL == hashkey) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid paramter specified \n",
						__FUNCTION__);
		errno = EINVAL;

		return NULL;
	}

	//memcpy((void *) &c.hashkey, (void *) hashkey, (SHA256_DIGEST_LENGTH + 1));

	pthread_spin_lock(&cache_lock);
	node = RBExactQuery(cache_tree, (void *) hashkey);
	if (NULL == node) {
		sfs_log(ctx, SFS_CRIT, "%s: hashkey %s not found in RB-tree \n",
						__FUNCTION__, (char *) hashkey);
		errno = EFAULT;
		pthread_spin_unlock(&cache_lock);

		return NULL;
	}
	c = node->info;
	pthread_spin_unlock(&cache_lock);

	pthread_mutex_lock(&(c->lock));
	if (c->on_ssd == true) {
		// Data is on SSD cache
		// Retrieve it
		if (c->ssdcache.len < size) {
			sfs_log(ctx, SFS_ERR, "%s: Incomplete data present on SSD cache "
							"for key %s \n", __FUNCTION__, hashkey);
			errno = EIO;
			pthread_mutex_unlock(&(c->lock));

			return NULL;
		}
		// Full data present on cache
		// Return the size
		if (ssd_cache && ssd_cache->ops.ssd_cache_retrieve) {
			data = ssd_cache->ops.ssd_cache_retrieve(c->ssdcache.handle,
							c->ssdcache.entry, size, ctx);
			if (NULL == data) {
				sfs_log(ctx, SFS_ERR, "%s: Unable to read complete data from "
								"SSD for hash %s \n", __FUNCTION__, hashkey);
				errno = EIO;
				pthread_mutex_unlock(&(c->lock));

				return NULL;
			}
		} else {
			sfs_log(ctx, SFS_ERR, "%s: No retrieve function defined for SSD !!"
							" . Memory corruption !!!. Hash %s \n",
							__FUNCTION__, hashkey);
			errno = EIO;

			return NULL;
		}
	} else {
		size_t datalen;
		// Data is on memcached
		data = sstack_memcache_read_one(c->memcache.mc, (const char *) hashkey,
						strlen(hashkey), &datalen, ctx);
		if (NULL == data) {
			sfs_log(ctx, SFS_ERR, "%s: Cache data is absent or incomplete on"
							"memcached for hash %s\n", __FUNCTION__, hashkey);
			errno = EIO;
			pthread_mutex_unlock(&(c->lock));

			return NULL;
		}
	}

	// SUCCESS
	pthread_mutex_unlock(&(c->lock));

	return data;
}

/*
 * sstack_cache_purge - Purge the entry corresponding to hashkey from
 *						cache
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
	int ret = -1;

	// Parameter validation
	if (NULL == hashkey) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid parameter specified \n",
						__FUNCTION__);
		errno = EINVAL;

		return -1;
	}

	//memcpy((void *) &c.hashkey, (void *) hashkey, (SHA256_DIGEST_LENGTH + 1));

	pthread_spin_lock(&cache_lock);
	node = RBExactQuery(cache_tree, (void *) hashkey);
	if (NULL == node) {
		sfs_log(ctx, SFS_CRIT, "%s: hashkey %s not found in RB-tree \n",
						__FUNCTION__, (char *) hashkey);
		errno = EFAULT;
		pthread_spin_unlock(&cache_lock);

		return -1;
	}
	pthread_spin_unlock(&cache_lock);

	entry = (sstack_cache_t *) node->info;
	pthread_mutex_lock(&entry->lock);
	if (entry->on_ssd == true) {
		// Cached on SSD
		if (ssd_cache && ssd_cache->ops.ssd_cache_purge) {
			ret = ssd_cache->ops.ssd_cache_purge(entry->ssdcache.handle,
							entry->ssdcache.entry, ctx);
			if (ret == -1) {
				sfs_log(ctx, SFS_ERR, "%s: SSD cache purge operation failed "
								"for hash %s \n", __FUNCTION__, hashkey);

				return -1;
			}
		} else {
			sfs_log(ctx, SFS_ERR, "%s: No purge function defined for SSD !!"
							" . Memory corruption !!!. Hash %s \n",
							__FUNCTION__, hashkey);
			errno = EIO;

			return -1;
		}
	} else {
		ret = sstack_memcache_remove(entry->memcache.mc,
					(const char *) entry->hashkey, ctx);
		if (ret == -1) {
			sfs_log(ctx, SFS_ERR, "%s: Removing cache entry from memcache "
							"failed for hash %s\n", __FUNCTION__, hashkey);
			return -1;
		}
	}

	// Remove entry from cache tree
	pthread_spin_lock(&cache_lock);
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

	cache->ops.ssd_cache_get_handle = (ssd_cache_get_handle_t)
			dlsym(handle, "sstack_ssd_cache_get_handle");
	if (NULL == cache->ops.ssd_cache_get_handle) {
		sfs_log(ctx, SFS_ERR, "%s: sstack_ssd_cache_get_handle not defined \n",
						__FUNCTION__);
		errno = EINVAL;
		free(cache);

		return NULL;
	}

	dlclose(handle);
	// NOTE:
	// There is no better place to initialize cache list lock. We need it
	// in ssd_cache_init itself.

	pthread_spin_init(&cache_list_lock, PTHREAD_PROCESS_PRIVATE);

	return cache;
}

