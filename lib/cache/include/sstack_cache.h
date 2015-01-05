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

#ifndef __SSTACK_CACHE_H__
#define __SSTACK_CACHE_H__

#include <inttypes.h>
#include <pthread.h>
#include <time.h>
#include <openssl/sha.h>
#include <stdbool.h>
#include <sstack_log.h>
#include <sstack_ssdcache.h>
#include <libmemcached/memcached.h>
#define FILENAME_LEN 32

typedef struct mcache {
	memcached_st *mc; // Memcached connection info
	size_t len; // Size of the object
} sstack_mcache_t;


// This file defines the generic cache structure

typedef struct sstack_cache {
	pthread_mutex_t lock;
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
extern char *sstack_cache_get(uint8_t *, size_t , log_ctx_t *);
extern int sstack_cache_purge(uint8_t *, log_ctx_t *);
extern int cache_init(log_ctx_t *);
extern ssd_cache_t *ssd_cache_register(log_ctx_t *ctx);

#endif // __SSTACK_CACHE_H__
