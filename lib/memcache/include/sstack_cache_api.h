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

#ifndef __SSTACK_CACHE_API_H_
#define __SSTACK_CACHE_API_H_
#include <libmemcached/memcached.h>
#include <sys/types.h>
#include <sstack_log.h>

// Check sstack_cache_api.c for details

memcached_st * sstack_memcache_init(const char *, int , log_ctx_t *);
void sstack_memcache_destroy(memcached_st *, log_ctx_t *);
int sstack_memcache_store(memcached_st *, const char *, const char *, size_t ,
				log_ctx_t *);
int sstack_memcache_remove(memcached_st *, const char * , log_ctx_t *);
char * sstack_memcache_read_one(memcached_st *, const char *, size_t , size_t *,
				log_ctx_t *);
int sstack_memcache_read_multiple(memcached_st *, const char **, size_t *, int ,
				log_ctx_t *);
int sstack_memcache_replace(memcached_st *, const char *, const char *,
		size_t , int , int , log_ctx_t *);

#endif // __SSTACK_CACHE_API_H_
