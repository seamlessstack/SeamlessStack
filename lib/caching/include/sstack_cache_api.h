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

#ifndef __SSTACK_CACHE_API_H_
#define __SSTACK_CACHE_API_H_
#include <libmemcached/memcached.h>
#include <sys/types.h>

// Check sstack_cache_api.c for details

memcached_st * sstack_cache_init(const char *, int );
void sstack_cache_destroy(memcached_st *);
int sstack_cache_store(memcached_st *, const char *, const char *, size_t );
char * sstack_cache_read_one(memcached_st *, const char *, size_t , size_t *);
int sstack_cache_read_multiple(memcached_st *, const char **, size_t *, int);
int sstack_cache_replace(memcached_st *, const char *, const char *,
		size_t , int , int );

#endif // __SSTACK_CACHE_API_H_
