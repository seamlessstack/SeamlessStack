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

#include <stdint.h>
#include <sstack_log.h>
#include <sstack_transport.h>

/*
 * TBD
 * Need to maintain a list of transports - one for each transport type
 * transport_register should return -1 (failure) is transport type is
 * already registered.
 */

inline int
sstack_transport_register(sstack_transport_type_t type,
					sstack_transport_t *transport,
					sstack_transport_ops_t ops)
{
	transport->transport_ops.client_init = ops.client_init;
	transport->transport_ops.rx = ops.rx;
	transport->transport_ops.tx = ops.tx;
	transport->transport_ops.server_setup = ops.server_setup;

	return 0;
}

inline int
sstack_transport_deregister(sstack_transport_type_t type,
						sstack_transport_t *transport)
{
	transport->transport_ops.client_init = NULL;
	transport->transport_ops.rx = NULL;
	transport->transport_ops.tx = NULL;
	transport->transport_ops.server_setup = NULL;

	return 0;
}
