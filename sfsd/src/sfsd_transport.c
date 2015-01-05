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
#include <sstack_sfsd.h>
#include <sstack_transport.h>
/* Temporary TODO: Remove */
sstack_client_handle_t tcp_client_init(sstack_transport_t *transport);


int32_t init_transport (sfsd_t *sfsd)
{
	sfsd->transport = get_tcp_transport(sfsd->sfs_addr, sfsd->log_ctx);
	sfsd->transport->ctx = sfsd->log_ctx;
	ASSERT((sfsd->transport != NULL), "Failed to fetch transport type TCP",
			1, 0, 0);
	sfsd->handle = tcp_client_init(sfsd->transport);
	sfs_log(sfsd->log_ctx, SFS_DEBUG, "%s: sfsd_handle %d\n",
				__FUNCTION__, sfsd->handle);
	ASSERT((sfsd->handle != 0), "Failed to get client transport handle",
			1, 0, 0);
	return 0;

}
