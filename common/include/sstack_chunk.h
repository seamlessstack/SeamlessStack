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

#ifndef __SSTACK_CHUNK_H_
#define __SSTACK_CHUNK_H_

#include <limits.h>
#include <sstack_jobs.h>
#include <sstack_storage.h>
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
	sfsd_state_t state; // Is current sfsd reachable?
	int num_chunks;
	sfsd_storage_t storage[0];
} sfs_chunk_domain_t;

#endif // __SSTACK_CHUNK_H_
