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
#include <red_black_tree.h>
#include <sstack_log.h>
#include <sstack_jobs.h>
#include <sstack_helper.h>
#include <sstack_cache.h>
#include <sstack_cache_api.h>

rb_red_blk_tree *cache_tree = NULL;

/*
 * compare_func - RB-tree compare function
 */
int
compare_func(const void *val1, const void *val2)
{
	sstack_cache_t *cache1 = (sstack_cache_t *) val1;
	sstack_cache_t *cache2 = (sstack_cache_t *) val2;


	// NOTE:
	// Hashkeys are generated using SHA256. So it is unlikely that
	// hashkeys will clash.
	switch(strcmp(cache1->hashkey, cache2->hashkey)) {
		case -1: return -1;
		case 1: return 1;
	}
}

/*
 * destroy_func - RB-tree node free function
 */
void
destroy_func(void *val)
{
	if (val)
		free(val);
}


/*
 * cache_init - Create RB-tree to maintain cache metadata
 *
 * Returns initialized cache_tree pointer upon success. Returns NULL upon
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

	return cache_tree;
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
		sfs_log(ctx, SFS_ERR, "%s: sstack_cache_store failed \n", __FUNCTION__);
		// FIXME
		// Check for out of memory condition
		// If so, use LRU map for eviction into SSD.
		return -1;
	}
	// memcache store succeeded.
	entry->on_ssd = false;
	entry->time = time(NULL);

	node = RBTreeInsert(cache_tree, (void *) entry->hashkey, (void *) entry);
	if (NULL == node) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to insert node into RB-tree \n",
						__FUNCTION__);
		sstack_cache_remove(entry->memcache.mc,
						(const char *) entry->hashkey, ctx);

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
	sstack_cache_t *ret = NULL;
	rb_red_blk_node *node = NULL;

	// Parameter validation
	if (NULL == hashkey) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid paramter specified \n",
						__FUNCTION__);

		errno = EINVAL;

		return NULL;
	}

	memcpy((void *) &c.hashkey, (void *) hashkey, (SHA256_DIGEST_LENGTH + 1));

	node = RBExactQuery(cache_tree, (void *) &c);
	if (NULL == node) {
		sfs_log(ctx, SFS_CRIT, "%s: hashkey %s not found in RB-tree \n",
						__FUNCTION__, (char *) hashkey);

		errno = EFAULT;

		return NULL;
	}

	return node->info;
}
