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

#include <sstack_jobs.h>
#include <limits.h>

// Chunk_domain data structure

typedef enum {
	NFS		= 1,  // *nix clients
	CIFS	= 2,  // Bidozzz clients
	ISCSI	= 3,  // iSCSI targets
	NATIVE	= 4,  // Native protocol to support faster storage access
} sfs_protocol_t;

// Individual storages can be reached by TCP/IP only.
// No other protocol supported at present.
typedef struct sfsd_storage {
	char path[PATH_MAX];
	sfs_protocol_t  protocol;
	union {
		char ipv4_addr[IPV4_ADDR_MAX];
		char ipv6_addr[IPV6_ADDR_MAX];
	};
} sfsd_storage_t;

typedef struct sfs_chunk_domain {
	sfsd_t *sfsd; // sfsd representing this chunk domain
	sfsd_state_t state; // Is current sfsd reachable?
	int num_chunks;
	sfsd_storage_t storage[0];
} sfs_chunk_domain_t;

#endif // __SSTACK_CHUNK_H_
