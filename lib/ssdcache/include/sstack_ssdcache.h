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
#ifndef __SSTACK_SSDCACHE_H__
#define __SSTACK_SSDCACHE_H__

#include <pthread.h>
#include <inttypes.h>
#include <sys/param.h>
#include <sstack_log.h>


// SSD cache operations
typedef uint32_t ssd_cache_handle_t;
typedef int32_t ssd_cache_entry_t;

typedef struct ssdcache {
	ssd_cache_handle_t handle;
	ssd_cache_entry_t entry;
	size_t len; // Size of the object
} sstack_ssdcache_t;

typedef ssd_cache_handle_t (*ssd_cache_init_t) (char *, log_ctx_t *);
// Returns the next SSD cache to be used for store
// Useful when there are multiple cache devices in the system
typedef ssd_cache_handle_t  (*ssd_cache_get_handle_t) (void);
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
	ssd_cache_get_handle_t ssd_cache_get_handle;
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
extern rb_red_blk_tree *lru_tree;
// Needs to be declared in calling application
extern ssd_cache_t *ssd_cache;

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

#endif // __SSTACK_SSDCACHE_H__
