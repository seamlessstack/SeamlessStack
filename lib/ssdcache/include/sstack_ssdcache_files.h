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

#ifndef __SSTACK_SSDCACHE_FILES_H__
#define __SSTACK_SSDCACHE_FILES_H__

// This file defines data structures used for maintaining SSD cache using files

#include <openssl/sha.h>
#include <sstack_log.h>
#include <red_black_tree.h>
#include <sstack_ssdcache.h>

#define CACHENAME_MAX (32 + FILENAME_MAX)

typedef struct ssdcache_entry {
	ssd_cache_entry_t ssd_ce;
	char name[CACHENAME_MAX];
} ssdcachemd_entry_t;

extern int ssdmd_add(ssd_cache_struct_t *, ssdcachemd_entry_t *);
extern int ssdmd_del(ssd_cache_struct_t *, ssdcachemd_entry_t *);

#endif // __SSTACK_SSDCACHE_FILES_H__



