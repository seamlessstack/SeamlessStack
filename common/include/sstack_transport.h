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

#ifndef __SSTACK_TRANSPORT_H_
#define __SSTACK_TRANSPORT_H_

#include <sstack_log.h>
#include <sstack_types.h>

#define IPV4_ADDR_MAX 16
#define IPV6_ADDR_MAX 40

#define SSTACK_BACKLOG 5

#define READ_BLOCK_MASK	1
#define WRITE_BLOCK_MASK 2

#define IGNORE_NO_BLOCK 0
#define READ_NO_BLOCK 1
#define WRITE_NO_BLOCK 2

#define MAX_RECV_RETRIES 10

typedef uint32_t sstack_handle_t;

typedef enum {
	TCPIP   = 1,
	IB  = 2,
	UDP = 3,
	UNIX    = 4,
} sstack_transport_type_t;

typedef struct sstack_transport sstack_transport_t; // Forward decl
sstack_transport_t* get_tcp_transport(char *addr);
/*
 *  init() is supposed to establish a connection and retrun client handle
 *  Client handle is socket fd i case of TCPIP
 */
typedef struct sstack_transport_ops {
	// First three are for client
	sstack_client_handle_t (*client_init) (sstack_transport_t *);
	int (*tx) (sstack_client_handle_t , size_t  , void *);
	int (*rx) (sstack_client_handle_t , size_t , void *);
	int (*select)(sstack_client_handle_t, uint32_t block_flags);
	// This one is for server side
	// Called only by sfs.
	sstack_client_handle_t  (*server_setup) (sstack_transport_t *);
} sstack_transport_ops_t;

typedef struct sstack_transport_hdr {
	union {
		struct tcp {
			int ipv4;       // 1 for ipv4 and 0 for ipv6
			union {
				char ipv4_addr[IPV4_ADDR_MAX];
				char ipv6_addr[IPV6_ADDR_MAX];
			};
			int port;
		} tcp;
	};
} sstack_transport_hdr_t;
	

struct sstack_transport {
	sstack_transport_type_t  transport_type;
	sstack_transport_hdr_t transport_hdr;
	sstack_transport_ops_t transport_ops;
	log_ctx_t *ctx;
};

static inline sstack_transport_t *
alloc_transport(void)
{
	sstack_transport_t *transport;

	transport = malloc(sizeof(sstack_transport_t));

	return transport;
}

static inline void
free_transport(sstack_transport_t *transport)
{
	if (transport)
		free(transport);
}


#endif // __SSTACK_TRANSPORT_H_
