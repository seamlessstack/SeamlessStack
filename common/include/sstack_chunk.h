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

#ifndef __SSTACK_CHUNK_H_
#define __SSTACK_CHUNK_H_

#include <limits.h>
#include <sstack_jobs.h>
#include <sstack_storage.h>
#include <sstack_log.h>
// Chunk_domain data structure
typedef enum {
	INITIALIZING 	= 1, // sfs sent a request to spawn sfsd
	RUNNING		= 2, // Handshake between sfs and sfsd is up
	REACHABLE	= 3, // Heartbeat successful
	UNREACHABLE	= 4, // Heartbeat dead. Could be n/w or sfsd
	DECOMMISSIONED	= 5, // Node running sfsd is decommissioned. A temp state
} sfsd_state_t;


typedef struct sfs_chunk_domain {
	sfsd_t *sfsd; // sfsd representing this chunk domain
	sfsd_state_t state; // Is current sfs reachable?
	uint32_t num_chunks; // Total number chunks in this chunk domain
	uint64_t size_in_blks; // Total size f chunk domain in file system blocks
	log_ctx_t  *ctx;	// Log context for the chunk domain
	sfsd_storage_t *storage; // Individual chunks
	/* The chunk scheduler function */
	int32_t (*schedule)(struct sfs_chunk_domain *);
} sfs_chunk_domain_t;


sfs_chunk_domain_t * sfsd_chunk_domain_init(sfsd_t *, log_ctx_t *);
void sfsd_chunk_domain_destroy(sfs_chunk_domain_t *);
char * sfsd_add_chunk(sfs_chunk_domain_t *, sfsd_storage_t *);
int sfsd_remove_chunk(sfs_chunk_domain_t *, sfsd_storage_t *);
int sfsd_update_chunk(sfs_chunk_domain_t *, sfsd_storage_t *);
void sfsd_display_chunk_domain(sfs_chunk_domain_t *);
#endif // __SSTACK_CHUNK_H_
