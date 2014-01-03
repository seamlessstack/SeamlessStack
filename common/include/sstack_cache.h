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
#define FILENAME_LEN 32

typedef struct mcache {
	size_t len; // Size of the object
	intptr_t ptr; // Memory location
} sstack_mcache_t;

// This file defines the generic cache structure

typedef struct sstack_cache {
	uint8_t hashkey[SHA256_DIGEST_LENGTH]; // Hash of file name and offset
	bool on_ssd; // Is it on SSD?
	union {
		char filename_on_ssd[FILENAME_LEN]; // Filename on SSD
		sstack_mcache_t memcache; // Memcached structure
	};
} sstack_cache_t;


// Functions
extern uint8_t * create_hash(void * , size_t , uint8_t *, log_ctx_t *);


#endif // __SSTACK_CACHE_H__


	


