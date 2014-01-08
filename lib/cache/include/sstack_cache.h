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
#ifndef __SSTACK_CACHE_H__
#define __SSTACK_CACHE_H__

#include <inttypes.h>
#include <openssl/sha.h>
#include <stdbool.h>
#include <sstack_log.h>
#include <libmemcached/memcached.h>
#include <time.h>
#define FILENAME_LEN 32

typedef struct mcache {
	memcached_st *mc; // Memcached connection info
	size_t len; // Size of the object
} sstack_mcache_t;


// This file defines the generic cache structure



// SSD cache operations
typedef uint32_t ssd_cache_handle_t;
typedef int32_t ssd_cache_entry_t;

typedef struct ssdcache {
	ssd_cache_handle_t handle;
	ssd_cache_entry_t entry;
} sstack_ssdcache_t;

typedef ssd_cache_handle_t (*ssd_cache_init_t) (char *, log_ctx_t *);
typedef int (*ssd_cache_open_t) (ssd_cache_handle_t , log_ctx_t *);
typedef void (*ssd_cache_close_t) (ssd_cache_handle_t , log_ctx_t *);
typedef ssd_cache_entry_t  (*ssd_cache_store_t) (ssd_cache_handle_t ,
				void *, size_t , log_ctx_t *);
typedef int (*ssd_cache_purge_t) (ssd_cache_handle_t , ssd_cache_entry_t ,
				log_ctx_t *);
typedef int (*ssd_cache_update_t) (ssd_cache_handle_t , ssd_cache_entry_t ,
				void *, size_t , log_ctx_t *);
typedef void * (*ssd_cache_retrieve_t) (ssd_cache_handle_t ,
				ssd_cache_entry_t , log_ctx_t *);
typedef void (*ssd_cache_destroy_t) (ssd_cache_handle_t );


typedef struct ssd_cache_ops {
	ssd_cache_init_t ssd_cache_init;
	ssd_cache_open_t ssd_cache_open;
	ssd_cache_close_t ssd_cache_close;
	ssd_cache_store_t ssd_cache_store;
	ssd_cache_purge_t ssd_cache_purge;
	ssd_cache_update_t ssd_cache_update;
	ssd_cache_retrieve_t ssd_cache_retrieve;
	ssd_cache_destroy_t ssd_cache_destroy;
} ssd_cache_ops_t;

// SSD cache structure
typedef struct ssd_cache {
	ssd_cache_ops_t ops;
	log_ctx_t *ctx;
} ssd_cache_t;

// Data structure to maintain multiple SSDs
typedef struct ssd_cache_struct {
	ssd_cache_handle_t handle;
	// FIXME:
	// SSD cache can be maintained in two ways:
	// 1. Use blocks directly and use SCSI READ/WRITE
	// 2. Use file system and store cache entries as files
	// Option 2 is easy and useful to us as we are going to cache
	// an extent at a time. This can be changed in future easily.
	// mnpnt here is for option 2.
	char mountpt[PATH_MAX];
	// FIXME:
	// If ssd_cache_ops differ with each SSD type, add ops into this
	// structure and add identifier(o/p of ATA IDENTIFY) to ops.
} ssd_cache_struct_t;

extern ssd_cache_struct_t ssd_caches[];
extern pthread_spinlock_t cache_list_lock;
extern ssd_cache_handle_t max_ssd_cache_handle;

static inline ssd_cache_handle_t
get_next_ssd_cache_handle(void)
{
	pthread_spin_lock(&cache_list_lock);
	max_ssd_cache_handle ++;
	pthread_spin_unlock(&cache_list_lock);

	return max_ssd_cache_handle;
}


// SSD cache functions
static inline ssd_cache_t *
create_ssd_cache(void)
{
	ssd_cache_t *ssd_cache = NULL;

	ssd_cache = malloc(sizeof(ssd_cache_t));

	return ssd_cache;
}

static inline void
destroy_ssd_cache(ssd_cache_t *ssd_cache)
{
	if (ssd_cache)
		free(ssd_cache);
}


// SSD cache registration function
static inline void
ssd_cache_register(ssd_cache_t *cache, ssd_cache_init_t cache_init,
		ssd_cache_open_t cache_open, ssd_cache_close_t cache_close,
		ssd_cache_store_t cache_store, ssd_cache_purge_t cache_purge,
		ssd_cache_update_t cache_update,
		ssd_cache_retrieve_t cache_retrieve,
		ssd_cache_destroy_t cache_destroy, log_ctx_t *ctx)
{
	if (cache) {
		cache->ops.ssd_cache_init = cache_init;
		cache->ops.ssd_cache_open = cache_open;
		cache->ops.ssd_cache_close = cache_close;
		cache->ops.ssd_cache_store = cache_store;
		cache->ops.ssd_cache_purge = cache_purge;
		cache->ops.ssd_cache_update = cache_update;
		cache->ops.ssd_cache_retrieve = cache_retrieve;
		cache->ops.ssd_cache_destroy = cache_destroy;
		cache->ctx = ctx;
	}
}

typedef struct sstack_cache {
	uint8_t hashkey[SHA256_DIGEST_LENGTH + 1]; // Hash of file name and offset
	bool on_ssd; // Is it on SSD?
	union {
		sstack_ssdcache_t ssdcache; // Cache entry for SSD cache	
		sstack_mcache_t memcache; // Memcached structure
	};
	time_t time; // Used for LRU impl

} sstack_cache_t;

//  Generic cache functions
extern uint8_t * create_hash(void * , size_t , uint8_t *, log_ctx_t *);
extern int sstack_cache_store(void *, size_t , sstack_cache_t *, log_ctx_t *);
extern sstack_cache_t *sstack_cache_search(uint8_t *, log_ctx_t *);
extern int sstack_cache_purge(uint8_t *, log_ctx_t *);
extern rb_red_blk_tree *  cache_init(log_ctx_t *);


#endif // __SSTACK_CACHE_H__
