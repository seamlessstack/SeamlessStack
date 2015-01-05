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
