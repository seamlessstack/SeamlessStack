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
