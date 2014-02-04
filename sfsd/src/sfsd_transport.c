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
