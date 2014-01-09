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

#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <openssl/sha.h>
#include <red_black_tree.h>
#include <sstack_log.h>
#include <sstack_lrucache.h>
#include <sstack_cache.h>

// This file implements generic LRU cache
// LRU caches can be implemented either by using a data structure sorted
// based on time. Idea is to use RB-tree and delete left most node each time
// a node needs to be evicted. Though tree is O(logN), it is still btter than
// a hash table with improper hash function.

rb_red_blk_tree *lru_tree = NULL;
pthread_spinlock_t lru_lock;

/*
 * compare_func - RB-tree compare function
 */

static int
lru_comp_func(const void *val1, const void *val2)
{
	if (*(time_t *)val1 > *(time_t *)val2)
		return 1;
	if (*(time_t *)val1 < *(time_t *)val2)
		return -1;

	return 0;
}

/*
 * lru_destroy_func - RB-tree node free function
 */

static void
lru_destroy_func(void *val)
{
	if (val)
		free(val);

}

/*
 * lru_init - Create RB-tree to maintain LRU cache
 *
 * Returns initialized lru_tree pointer om success. Retruns NULL upon
 * failure.
 */

rb_red_blk_tree *
lru_init(log_ctx_t *ctx)
{
	// Create RB-tree for managing the cache
	lru_tree = RBTreeCreate(lru_comp_func, lru_destroy_func, NULL,
					NULL, NULL);
	if (NULL == lru_tree)
		sfs_log(ctx, SFS_ERR, "%s: Failed to create RB-tree for LRU "
						"metadata.\n", __FUNCTION__);

	pthread_spin_init(&lru_lock, PTHREAD_PROCESS_PRIVATE);

	return lru_tree;
}

/*
 * lru_insert_entry - Insert LRU information into LRU RB-tree
 *
 * hashval - Hashkey representing the cache entry
 * ctx - log context
 *
 * Returns 0 on success and -1 on failure.
 */

int
lru_insert_entry(rb_red_blk_tree *tree, void *hashval, log_ctx_t *ctx)
{
	sstack_lru_entry_t *entry = NULL;
	rb_red_blk_node *node = NULL;
	time_t t;
	// Parameter validation
	if (NULL == hashval || NULL == tree) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);
		errno = EINVAL;

		return -1;
	}

	// Insert element into RB-tree
	entry = calloc(sizeof(sstack_lru_entry_t), 1);
	if (NULL == entry) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to allocate memory for "
						"LRU entry. Hash val = %s\n", __FUNCTION__, hashval);

		return -1;
	}
	strncpy(entry->hashkey, (const char *)hashval, SHA256_DIGEST_LENGTH);
	t = time(NULL);

	pthread_spin_lock(&lru_lock);
	node = RBTreeInsert(tree, (void *) t, (void *) entry);
	if (NULL == node) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to insert into RB-tree."
						" Hashval = %s \n", __FUNCTION__, hashval);
		pthread_spin_unlock(&lru_lock);

		return -1;
	}
	pthread_spin_unlock(&lru_lock);

	return 0;
}

/*
 * leftmost_node - Helper function to return leaf element with least value
 *
 * tree - RB-tree
 *
 * Returns left-most node on success and NULL on failure.
 */

static rb_red_blk_node *
leftmost_node(rb_red_blk_tree *tree)
{
	rb_red_blk_node *node;

	if (NULL == tree)
		return NULL;

	pthread_spin_lock(&lru_lock);

	node = tree->root;

	// If there are no left nodes in the tree, return the root of
	// the subtree (current node) itself as the value in their is
	// lesser than the node on its right.
	while (NULL != node->left)
		node = node->left;
	pthread_spin_unlock(&lru_lock);

	return node;
}

/*
 * lru_demote_entries - Demotes least recently used entries from
 *						the cache
 *
 * tree - LRU RB-tree
 * num - Number of elements to delete
 * ctx - Log context
 *
 * Deletes num entries from LRU RB-tree and tries to store them on
 * SSD cache if SSD cache is registered. If not, purges the cache
 * entries.
 * Returns 0 on success and -1 on failure.
 * FIXME:
 * Error conditions need to be handled properly.
 */

int
lru_demote_entries(rb_red_blk_tree *tree, int num, log_ctx_t *ctx)
{
	int i;
	int ret = -1;

	// Parameter validation
	if (NULL == tree || num <= 0) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);
		errno = EINVAL;

		return -1;
	}

	// Delete num entries from the tree
	for (i = 0; i < num; i++) {
		rb_red_blk_node *node;
		sstack_lru_entry_t *entry;
		sstack_cache_t *cache = NULL;
		char *data = NULL;
		size_t data_len = -1;

		node = leftmost_node(tree);
		if (NULL == node) {
			sfs_log(ctx, SFS_ERR, "%s: LRU RB-tree has no elements! \n",
							__FUNCTION__);

			return -1;
		}

		// Remove entry from memcached cache
		entry = node->info;
		cache = sstack_cache_search(entry->hashkey, ctx);
		if (NULL == cache) {
			sfs_log(ctx, SFS_ERR, "%s: Failed to get cache information"
					" for hashkey %s \n", __FUNCTION__, entry->hashkey);
			// Delete the node as relevant info is not found
			RBDelete(tree, node);

			return -1;
		}

		pthread_spin_lock(&cache->lock);

		// Get data stored in memcached
		data = sstack_memcache_read_one(cache->memcache.mc,
						(const char *) entry->hashkey,
						strlen(entry->hashkey),
						&data_len, ctx);
		if (NULL == data) {
			sfs_log(ctx, SFS_ERR, "%s: Failed to read cached data "
							"in memcached. \n", __FUNCTION__);
			(void) sstack_cache_purge(entry->hashkey, ctx);
			RBDelete(tree, node);
			pthread_spin_unlock(&cache->lock);

			return -1;
		}


		// Check if SSD cache is enabled
		// If so, push the entries into SSD cache
		if (ssd_cache && ssd_cache->ops.ssd_cache_store) {
			sstack_cache_t *cache = NULL;
			ssd_cache_entry_t ce;
			ssd_cache_handle_t handle;

			// Get the right SSD to store data
			handle = ssd_cache->ops.ssd_cache_get_handle();
			if (handle == -1) {
				sfs_log(ctx, SFS_ERR, "%s: Failed to get cache handle. "
						"Either SSD cache is full or fatal error \n",
						__FUNCTION__);
				pthread_spin_unlock(&cache->lock);

				return -1;
			}
			// Store the data into SSD
			ce = ssd_cache->ops.ssd_cache_store(handle, (void *) data,
							data_len, ctx);
			if (ce < 0) {
				sfs_log(ctx, SFS_ERR, "%s: Failed to store data into "
						"ssd cache. Error = %d \n",
						__FUNCTION__, errno);
				pthread_spin_unlock(&cache->lock);

				return -1;
			}
			// Remove the entry from memcached
			ret =  sstack_memcache_remove(cache->memcache.mc,
							entry->hashkey, ctx);
			if (ret == -1) {
				sfs_log(ctx, SFS_ERR, "%s: Unable to evict entry from"
								" memcached\n", __FUNCTION__);
				pthread_spin_unlock(&cache->lock);

				return -1;
			}
			// Update cache entry
			cache->on_ssd = true;
			cache->ssdcache.handle = handle;
			cache->ssdcache.entry = ce;
			cache->ssdcache.len = data_len;
			// FIXME:
			// Ther is is no way to update an existing entry other than
			// remove and add back.

			ret = sstack_cache_purge(entry->hashkey, ctx);
			if (ret == -1) {
				sfs_log(ctx, SFS_ERR, "%s: Failed to purge entry from "
								"cache tree \n", __FUNCTION__);
				pthread_spin_unlock(&cache->lock);

				return -1;
			}

			ret = sstack_cache_store((void *) data, data_len, cache,
							ctx);
			if (ret == -1) {
				sfs_log(ctx, SFS_ERR, "%s: Failed to store data into "
								"memcached \n", __FUNCTION__);
				pthread_spin_unlock(&cache->lock);

				return -1;
			}
			pthread_spin_unlock(&cache->lock);
		} else {
			// Remove the entry from memcached
			ret =  sstack_memcache_remove(cache->memcache.mc,
							entry->hashkey, ctx);
			if (ret == -1) {
				sfs_log(ctx, SFS_ERR, "%s: Unable to evict entry from"
								" memcached\n", __FUNCTION__);
				pthread_spin_unlock(&cache->lock);

				return -1;
			}
			// Remove entry from cache tree
			ret = sstack_cache_purge(entry->hashkey, ctx);
			if (ret == -1) {
				sfs_log(ctx, SFS_ERR, "%s: Failed to purge entry from "
								"cache tree \n", __FUNCTION__);
				pthread_spin_unlock(&cache->lock);

				return -1;
			}
			// Remove entry from LRU tree
			RBDelete(tree, node);
		}
	}

	return 0;
}
