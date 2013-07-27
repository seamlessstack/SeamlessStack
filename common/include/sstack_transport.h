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

#define IPV4_ADDR_MAX 16
#define IPV6_ADDR_MAX 40

typedef enum {
	TCPIP   = 1,
	IB  = 2,
	UDP = 3,
	UNIX    = 4,
} sstack_transport_type_t;

/*
 *  init() is supposed to establish a connection and retrun client handle
 *  Client handle is socket fd i case of TCPIP
 */
typedef struct sstack_transport_ops {
	// First three are for client
	sstack_client_handle_t (*client_init) (sstack_transport_t *);
	int (*tx) (sstack_client_handle_t , size_t  , void *);
	int (*rx) (sstack_client_handle_t , size_t , void *);
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
	

typedef struct sstack_transport {
	sstack_transport_type_t  transport_type;
	sstack_transport_hdr_t transport_hdr;
	sstack_transport_ops_t ops;
	log_ctx_t *ctx;
} sstack_transport_t;

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

/*
 * TBD
 * Need to maintain a list of transports - one for each transport type
 * transport_register should return -1 (failure) is transport type is
 * already registered.
 */

static inline int
sstack_transport_register(sstack_transport_type_t type,
					sstack_sstack_transport_t *transport,
					sstack_transport_ops_t ops)
{
	transport->transport_ops.client_init = ops.client_init;
	transport->transport_ops.rx = ops.rx;
	transport->transport_ops.tx = ops.tx;
	transport->transport_ops.server_setup = ops.server_setup;

	return 0;
}

#endif // __SSTACK_TRANSPORT_H_
