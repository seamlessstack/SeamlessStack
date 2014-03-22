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

#ifndef __SSTACK_TYPES_H_
#define __SSTACK_TYPES_H_

#include <inttypes.h>

// All sstack specific types should be in this file
#define IPV4_ADDR_MAX 16
#define IPV6_ADDR_MAX 40
typedef uint64_t sstack_client_handle_t;
typedef uint64_t sstack_job_id_t;
typedef uint8_t sstack_bitmap_t;
typedef uint32_t sstack_cksum_t;

/* Supported Protocols */
typedef enum {
	NFS	= 1,  // *nix clients
	CIFS	= 2,  // Bidozzz clients
	ISCSI	= 3,  // iSCSI targets
	NATIVE	= 4,  // Native protocol to support faster storage access
} sfs_protocol_t;

typedef enum {
	IPV4 = 1,
	IPV6 = 2,
} sstack_network_protocol_t ;

/*
 * Changing this struture would affect cli structures, specifically
 * struct storage_input in cli/include/sfscli.h
 */
typedef struct {
	sstack_network_protocol_t protocol;
	union {
		char ipv4_address[IPV4_ADDR_MAX];
		char ipv6_address[IPV6_ADDR_MAX];
	};
	/* Used in CLI. Lists the number of field
	   the structure has. Please update this when
	   changing the structure
	*/
#define NUM_SSTACK_ADDRESS_FIELDS 2
} sstack_address_t;

typedef enum {
	SSTACK_STORAGE_HDD = 1,
	SSTACK_STORAGE_SSD = 2,
} sstack_storage_type_t;

typedef enum sstack_return_code {
	SSTACK_SUCCESS = 100,
	SSTACK_FAILURE = 101, // other than cksum and no-memory
	SSTACK_ECKSUM = 102, // Indicates checksum does not match
	SSTACK_NOMEM = 103, // Indicates no-memory
	SSTACK_CRIT_FAILURE = 104, // any internal critical error
} sstack_ret_code_t;


#define MAX_EXTENT_SIZE 65536 /* 64 * 1024 bytes */
#endif // __SSTACK_TYPES_H_
