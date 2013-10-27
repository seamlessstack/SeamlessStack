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

#ifndef __SSTACK_TYPES_H_
#define __SSTACK_TYPES_H_

// All sstack specific types should be in this file
#define IPV4_ADDR_MAX 16
#define IPV6_ADDR_MAX 40
typedef uint64_t sstack_client_handle_t;
typedef uint64_t sstack_job_id_t;

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

typedef struct {
	sstack_network_protocol_t protocol;
	union {
		char ipv4_address[IPV4_ADDR_MAX];
		char ipv6_address[IPV6_ADDR_MAX];
	};
} sstack_address_t;

#define MAX_EXTENT_SIZE 65536 /* 64 * 1024 bytes */
#endif // __SSTACK_TYPES_H_
