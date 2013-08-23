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

#ifndef __SSTACK_STORAGE_H_
#define __SSTACK_STORAGE_H_

#define MAX_MOUNT_POINT_LEN 16

/* Supported Protocols */
typedef enum {
	NFS	= 1,  // *nix clients
	CIFS	= 2,  // Bidozzz clients
	ISCSI	= 3,  // iSCSI targets
	NATIVE	= 4,  // Native protocol to support faster storage access
} sfs_protocol_t;

typedef struct sfsd_storage {
	char path[PATH_MAX];		// Path of the source
	char mount_point[MAX_MOUNT_POINT_LEN]; // Local mount point - not persisted
	uint32_t weight;			// Storage weight
	uint64_t nblocks; 		// Number of file system blocks
	sfs_protocol_t  protocol;	// How this chunk is reached?
	// Chunk's "address"
	union {
		char ipv4_addr[IPV4_ADDR_MAX]; // NULL terminated address
		char ipv6_addr[IPV6_ADDR_MAX]; // NULL terminated address
		/* Other protocol addresses */
	};
} sfsd_storage_t;
#endif /*__SSTACK_STORAGE_H_ */
