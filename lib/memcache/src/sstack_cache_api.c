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

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <libmemcached/memcached.h>
#include <sstack_log.h>
#define MEMCACHED_PORT 11211
#define SEPERATOR ":"

/*
 * sstack_cache_init - Creates memcached_st handle 
 *
 * config - Memcached server locations in the form
 * 			 "server1:server2:..."
 * num_servers - Number of memcached servers in the pool.
 * ctx - Log context
 *
 * Returns memcached_st pointer on sucess. Returns NULL on failure
 */

memcached_st *
sstack_cache_init(const char *config, int num_servers, log_ctx_t *ctx)
{
	memcached_server_st *servers = NULL;
	memcached_st *memc = NULL;
	memcached_return_t rc;

	memc = memcached_create(NULL);
	if (NULL == memc) {
		sfs_log(ctx, SFS_ERR, "%s: Unable to create memcached connection\n",
						__FUNCTION__);

		return NULL;
	}

	if (num_servers > 1) {
		char *buf = strdup(config);
		char **ptr = (char **) &buf;
		char *server_name = NULL;

		while((server_name = strsep(ptr, SEPERATOR)) != NULL) {
			if (strlen(server_name) == 0)
				continue;
			servers = memcached_server_list_append(servers, server_name,
				MEMCACHED_PORT, &rc);
			rc = memcached_server_push(memc, servers);
			if (rc != MEMCACHED_SUCCESS) {
				sfs_log(ctx, SFS_ERR, "%s: memcached_server_push failed\n",
								__FUNCTION__);

				return NULL;
			}
		}
		free(buf);
	} else {
		servers = memcached_server_list_append(servers, config,
				MEMCACHED_PORT, &rc);
		rc = memcached_server_push(memc, servers);
		if (rc != MEMCACHED_SUCCESS) {
			sfs_log(ctx, SFS_ERR, "%s: memcached_server_push failed\n",
							__FUNCTION__);

			return NULL;
		}
	}

	sfs_log(ctx, SFS_INFO, "%s: memcahed init succeeded\n", __FUNCTION__);

	return memc;
}

/*
 * sstack_cache_destroy -  Destroys memcached connectiom
 *
 * memc - memcached_st pointer
 * ctx - log ctx
 *
 * Returns immediately if parameters are invalid.
 */

inline void
sstack_cache_destroy(memcached_st *memc, log_ctx_t *ctx)
{
	if (NULL == memc) {
		sfs_log(ctx, SFS_ERR, "%s: memc is NULL \n", __FUNCTION__);

		return;
	}
	memcached_free(memc);
	sfs_log(ctx, SFS_INFO, "%s: Memcached database destroyed \n", __FUNCTION__);
}


/*
 * sstack_cache_store - Store a key:value pair on memcached
 *
 * memc - memcached_st pointer obtained by a call to sstack_cache_init
 * key - Null terminated string that stores key
 * value - binary data, need not be NULL terminated
 * valuelen - Length of value in bytes
 * ctx - log ctx
 *
 * Returns 0 on sucess and negative integer on failure
 */

int
sstack_cache_store(memcached_st *memc,
				const char *key,
				const char *value,
				size_t valuelen,
				log_ctx_t *ctx)
{
	memcached_return_t rc = -1;

	if (NULL == key || NULL == value || valuelen <= 0 || NULL == memc) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);

		return -1;
	}
	rc = memcached_set(memc, key, strlen(key), value, valuelen,
			(time_t)0, (uint32_t) 0); 
	if (rc != MEMCACHED_SUCCESS) {
		sfs_log(ctx, SFS_ERR, "%s: Storing key %s n memcahced db failed with"
						" error %d\n", __FUNCTION__, key, rc);

		return -rc;
	} else {
		sfs_log(ctx, SFS_INFO, "%s: stored key %s into memcached db \n",
					__FUNCTION__, key);

		return 0;
	}
}

/*
 * sstack_cache_remove - Remove key from memcached
 *
 * memc - memcached connection handle
 * key - key to be deleted
 * ctx - log ctx
 *
 * Deletes teh key from the DB
 * Retruns 0 on success and negative number upon failure.
 */

int
sstack_cache_remove(memcached_st *memc, const char *key, log_ctx_t *ctx)
{
	int rc = -1;

	// Parameter validation
	if (NULL == key || NULL == memc) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);

		return -1;
	}
	rc = memcached_delete(memc, key, strlen(key), 0);
	if (rc != MEMCACHED_SUCCESS) {
		sfs_log(ctx, SFS_ERR, "%s: memcached_delete for key %s failed with "
						"error %d \n", __FUNCTION__, key, rc);

		return -rc;
	} else {
		sfs_log(ctx, SFS_INFO, "%s: Key %s deleted from memcached db \n",
						__FUNCTION__, key);

		return 0;
	}
}

/*
 * sstack_cache_read_one - Read value corresponding to a key
 *
 * memc - memcached_st pointer
 * valuelen - Return value that indicates size of the value retruned
 * ctx - log ctx
 *
 * Returns the object (whose size is determoned by *valuelen) on success
 * and returns NULL on failure.
 */

char *
sstack_cache_read_one(memcached_st *memc,
					const char *key,
					size_t keylen,
					size_t *valuelen,
					log_ctx_t *ctx)
{
	if (NULL == memc || NULL == key || keylen == 0 || NULL == valuelen) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid parameters specified\n",
						__FUNCTION__);

		return NULL;
	}

	return memcached_get(memc, key, keylen, valuelen,
			(uint32_t *)NULL, (memcached_return_t *) NULL);
}

/*
 * sstack_cache_read_multiple - Read multiple keys at once
 *
 * memc - memcached_st pointer
 * keys - array of strings holding keys
 * keylen - array key length 
 * num_keys - number of keys to fetch
 * ctx - log ctx
 *
 * Returns 0 on success and negative number on failure
 *
 * NOTE:
 * Upon successful return, use the following code to get the individual values:
 *
 * char return_key[MEMCACHED_MAX_KEY];
 * size_t return_key_length;
 * unsigned int x = 0;
 * uint32_t flags;
 *
 * while ((return_value = memcached_fetch(memc, return_key, &return_key_length,
 *					&return_value_length, &flags, &rc)))
 * {
 *		// Do whatever needs to be done with the value
 *		free (return_value);
 *		x++;
 * } 
 */

int
sstack_cache_read_multiple(memcached_st * memc,
						const char **keys,
						size_t *keylen,
						int num_keys,
						log_ctx_t *ctx)
{
	memcached_return_t rc = MEMCACHED_FAILURE;

	if (NULL == memc || NULL == *keys || num_keys <= 0) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);

		return -1;
	}
	rc = memcached_mget(memc, keys, keylen, num_keys);
	if (rc == MEMCACHED_SUCCESS)
		return 0;
	else
		return -rc;
}


/*
 * sstack_cache_replace - Replace value for the given key
 *
 * memc - memcached_st pointer
 * expiration - indicates when this object expires
 * ctx - log ctx
 *
 * Returns 0 on success and negative number on failure.
 */

int
sstack_cache_replace(memcached_st *memc,
					const char *key,
					const char *value,
					size_t valuelen,
					time_t expiration,
					uint32_t flags,
					log_ctx_t *ctx)
{

	memcached_return_t ret;

	if (NULL == memc || NULL == key) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);

		return -1;
	}

	ret = memcached_replace(memc, key, strlen(key), value, valuelen,
			expiration, flags);
	if (ret != MEMCACHED_SUCCESS) {
		sfs_log(ctx, SFS_ERR, "%s: Replacing value of key %s failed. "
						"Attempted new value = %s. Error = %d\n",
						__FUNCTION__, key, value, ret);

		return -ret;
	} else {
		sfs_log(ctx, SFS_INFO, "%s: Successfully replaced value of key %s ."
						"New value = %s \n", __FUNCTION__, key, value);

		return 0;
	}
}
