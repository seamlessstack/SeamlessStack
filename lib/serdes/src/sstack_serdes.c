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
#include <string.h>
#include <stdbool.h>
#include <sys/errno.h>
#include <sstack_serdes.h>
#include <jobs.pb-c.h>

static uint32_t sequence = 1; // Sequence number for packets

/*
 * sstack_send_payload - Send payload to client represented by handle
 *
 * handle - client representation
 * payload - non-serialized payload
 * transport - transport between sfs and sfsd
 * ctx - log context
 *
 * Takes payload , serializes the payload using Google protocol buffers
 * and calls transport function as specified in transport.
 * This is used by both sfs and sfsd. For sfs -> sfsd,  syload is either
 * storage/chunk commands or NFS commands. For sfsd -> sfs, payload is
 * response for earlier submitted command.
 *
 * Returns 0 on success and negatuve number on failure.
 */

int
sstack_send_payload(sstack_client_handle_t handle,
					sstack_payload_t * payload,
					sstack_transport_t *transport,
					log_ctx_t *ctx)
{

	SstackPayloadT msg = SSTACK_PAYLOAD_T__INIT;
	SstackPayloadHdrT hdr = SSTACK_PAYLOAD_HDR_T__INIT;
	uint32_t payload_len = 0;

	hdr.sequence = sequence++;
	hdr.payload_len = 0; // Dummy . will be filled later
	msg.hdr = &hdr;
	switch (payload->command) {
		case SSTACK_ADD_STORAGE: {
			SfsdStorageT storage = SFSD_STORAGE_T__INIT;
			size_t len = 0;
			char * buffer = NULL;

			sfs_log(ctx, SFS_INFO, "%s: SSTACK_ADD_STORAGE called with "
				"path = %s  ipaddr = %s\n",
				__FUNCTION__, payload->storage.path,
				(transport->transport_hdr.tcp.ipv4) ?
					payload->storage.ipv4_addr : payload->storage.ipv6_addr);
			msg.command =
				SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__SSTACK_ADD_STORAGE;	
			storage.path.len = strlen(payload->storage.path);
			strcpy((char *) storage.path.data, payload->storage.path);
			storage.mount_point.len = strlen(payload->storage.mount_point);
			strcpy((char *)storage.mount_point.data,
							payload->storage.mount_point);
			storage.weight = payload->storage.weight;
			storage.nblocks = payload->storage.nblocks;
			storage.protocol = payload->storage.protocol;
			if (transport->transport_hdr.tcp.ipv4) {
				storage.has_ipv4_addr = true;
				storage.ipv4_addr.len = IPV4_ADDR_MAX;
				strcpy((char *) storage.ipv4_addr.data,
					payload->storage.ipv4_addr);
			} else {
				storage.has_ipv6_addr = true;
				storage.ipv6_addr.len = IPV6_ADDR_MAX;
				strcpy((char *) storage.ipv6_addr.data,
					payload->storage.ipv6_addr);
			}
			msg.storage = &storage;
			len = sstack_payload_t__get_packed_size(&msg);
			hdr.payload_len = len - sizeof(sstack_payload_hdr_t);
			msg.hdr = &hdr; // Parannoid 
			buffer = malloc(len);
			if (NULL == buffer) {
				sfs_log(ctx, SFS_ERR, "%s: Failed to allocate memory for "
					"adding storage. Command aborted \n", __FUNCTION__);
				return -ENOMEM;
			}
			sstack_payload_t__pack(&msg, buffer);
			// Send it using transport
			if (transport && transport->transport_ops.tx && (handle > 0)) {
				int ret = -1;

				ret = transport->transport_ops.tx(handle, len, (void *) buffer);
				if (ret == -1) {
					sfs_log(ctx, SFS_ERR, "%s: Failed to xmit payload "
						"for SSTACK_ADD_STORAGE request for ipaddr %s "
						"path %s \n", __FUNCTION__, 
						(transport->transport_hdr.tcp.ipv4) ?
							payload->storage.ipv4_addr :
							payload->storage.ipv6_addr,
						payload->storage.path);
					free(buffer);

					return -errno;
				}
				// Assumption is tx will try few times to transmit full data.
				// If it is unsuccessul, tx returns -1.
				// So not checking for partial data transfer here.
				sfs_log(ctx, SFS_INFO, "%s: Successfully xmitted payload "
					"for SSTACK_ADD_STORAGE request for ipaddr %s "
					"path %s \n", __FUNCTION__,
					(transport->transport_hdr.tcp.ipv4) ?
						payload->storage.ipv4_addr : payload->storage.ipv6_addr,
					payload->storage.path);
				free(buffer);

				return 0;
			} else {
				sfs_log(ctx, SFS_ERR, "%s: Failed to xmit payload "
					"for SSTACK_ADD_STORAGE request for ipaddr %s "
					"path %s \n", __FUNCTION__, 
					(transport->transport_hdr.tcp.ipv4) ?
						payload->storage.ipv4_addr : payload->storage.ipv6_addr,
					payload->storage.path);
				free(buffer);

				return -EINVAL;
			}
		}
	}

	return 0;
}


sstack_payload_t *
sstack_recv_payload(sstack_client_handle_t handle,
					sstack_transport_t *transport,
					log_ctx_t *ctx)
{

	return NULL;
}
