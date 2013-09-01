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
 * sstack_command_stringify - blurt out the command in string
 *
 * This is needed to make the send and recv code better.
 * Otherwise, a totally dummy function.
 */

static inline char *
sstack_command_stringify(sstack_command_t command)
{
	switch(command) {
		case SSTACK_ADD_STORAGE: return "SSTACK_ADD_STORAGE";
		case SSTACK_REMOVE_STORAGE: return "SSTACK_REMOVE_STORAGE";
		case SSTACK_UPDATE_STORAGE: return "SSTACK_UPDATE_STORAGE";
		case SSTACK_ADD_STORAGE_RSP: return "SSTACK_ADD_STORAGE_RSP";
		case SSTACK_REMOVE_STORAGE_RSP: return "SSTACK_REMOVE_STORAGE_RSP";
		case SSTACK_UPDATE_STORAGE_RSP: return "SSTACK_UPDATE_STORAGE_RSP";
		case NFS_GETATTR: return "NFS_GETATTR";
		case NFS_SETATTR: return "NFS_SETATTR";
		case NFS_LOOKUP: return "NFS_LOOKUP";
		case NFS_ACCESS: return "NFS_ACCESS";
		case NFS_READLINK: return "NFS_READLINK";
		case NFS_READ: return "NFS_READ";
		case NFS_WRITE: return "NFS_WRITE";
		case NFS_CREATE: return "NFS_CREATE";
		case NFS_MKDIR: return "NFS_MKDIR";
		case NFS_SYMLINK: return "NFS_SYMLINK";
		case NFS_MKNOD: return "NFS_MKNOD";
		case NFS_REMOVE: return "NFS_REMOVE";
		case NFS_RMDIR: return "NFS_RMDIR";
		case NFS_RENAME: return "NFS_RENAME";
		case NFS_LINK: return "NFS_LINK";
		case NFS_READDIR: return "NFS_READDIR";
		case NFS_READDIRPLUS: return "NFS_READDIRPLUS";
		case NFS_FSSTAT: return "NFS_FSSTAT";
		case NFS_FSINFO: return "NFS_FSINFO";
		case NFS_PATHCONF: return "NFS_PATHCONF";
		case NFS_COMMIT: return "NFS_COMMIT";
		case NFS_GETATTR_RSP: return "NFS_GETATTR_RSP";
		case NFS_LOOKUP_RSP: return "NFS_LOOKUP_RSP";
		case NFS_ACCESS_RSP: return "NFS_ACCESS_RSP";
		case NFS_READLINK_RSP: return "NFS_READLINK_RSP";
		case NFS_READ_RSP: return "NFS_READ_RSP";
		case NFS_WRITE_RSP: return "NFS_WRITE_RSP";
		case NFS_CREATE_RSP: return "NFS_CREATE_RSP";
	}
}

/*
 * _send_payload - Helper function to call transport function
 * Main itention is to avoid clutter for each command handling.
 *
 * transport - transport structure used between sfs and sfsd
 * handle - client handle. Should be non-NULL.
 * buffer - preallocated buffer containing packed payload
 * len - Length of the packed payload.
 *
 * Return 0 on success and negative number indicating error on failure.
 */ 

static inline int
_send_payload(sstack_transport_t *transport,
			sstack_client_handle_t handle,
			char *buffer,
			size_t len)
{

	if (transport && transport->transport_ops.tx && (handle > 0)) {
		int ret = -1;

		ret = transport->transport_ops.tx(handle, len, (void *) buffer);
		if (ret == -1) {
			// Request failed
			return -errno;
		}
		// Assumption is tx will try few times to transmit full data.
		// If it is unsuccessul, tx returns -1.
		// So not checking for partial data transfer here.

		return 0;
	} else {
		// Something wrong with parameters
		return -EINVAL;
	}
}

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
	SstackNfsCommandStruct cmd = SSTACK_NFS_COMMAND_STRUCT__INIT;
	uint32_t payload_len = 0;
	int ret = -1;

	hdr.sequence = sequence++;
	hdr.payload_len = 0; // Dummy . will be filled later
	msg.hdr = &hdr;
	switch (payload->command) {
		case SSTACK_ADD_STORAGE:
		case SSTACK_REMOVE_STORAGE:
		case SSTACK_UPDATE_STORAGE: {
			SfsdStorageT storage = SFSD_STORAGE_T__INIT;
			size_t len = 0;
			char * buffer = NULL;

			// TODO
			// For SSTACK_UPDATE_STORAGE, need to print previous and new
			// values. It appears we have only nblocks as target for update
			// as of now. May be there will be requirement to change other
			// fields in future. So not logging the request now. REVISIT
			sfs_log(ctx, SFS_INFO, "%s: %s called with "
				"path = %s  ipaddr = %s\n",
				__FUNCTION__, sstack_command_stringify(payload->command),
				payload->storage.path,
				(transport->transport_hdr.tcp.ipv4) ?
					payload->storage.ipv4_addr : payload->storage.ipv6_addr);
			if (payload->command == SSTACK_ADD_STORAGE)
				msg.command =
					SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__SSTACK_ADD_STORAGE;	
			else if (payload->command == SSTACK_REMOVE_STORAGE)
				msg.command =
				 SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__SSTACK_REMOVE_STORAGE;	
			else
				msg.command =
				 SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__SSTACK_UPDATE_STORAGE;	
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
					"%s path %s. Command aborted \n",
					__FUNCTION__, sstack_command_stringify(payload->command),
					payload->storage.path);
				return -ENOMEM;
			}
			sstack_payload_t__pack(&msg, buffer);
			// Send it using transport
			ret = _send_payload(transport, handle, buffer, len);
			if (ret != 0) {
				sfs_log(ctx, SFS_ERR, "%s: Failed to xmit payload "
					"for %s request for ipaddr %s "
					"path %s. Error = %d\n", __FUNCTION__, 
					sstack_command_stringify(payload->command),
					(transport->transport_hdr.tcp.ipv4) ?
						payload->storage.ipv4_addr :
						payload->storage.ipv6_addr,
					payload->storage.path, ret);

				free(buffer);

				return -ret;
			} else {
				sfs_log(ctx, SFS_INFO, "%s: Successfully xmitted payload "
					"for %s request for ipaddr %s "
					"path %s \n", __FUNCTION__,
					sstack_command_stringify(payload->command),
					(transport->transport_hdr.tcp.ipv4) ?
						payload->storage.ipv4_addr :
						payload->storage.ipv6_addr,
					payload->storage.path);

				free(buffer);

				return 0;
			}
		} 
		case NFS_SETATTR: {
			SstackNfsSetattrCmd setattrcmd = SSTACK_NFS_SETATTR_CMD__INIT;
			SstackFileAttributeT fileattr = SSTACK_FILE_ATTRIBUTE_T__INIT;
			size_t len = 0;
			char * buffer = NULL;

			sfs_log(ctx, SFS_INFO, "%s: %s called \n", __FUNCTION__,
					sstack_command_stringify(payload->command));
			msg.command = SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__NFS_SETATTR;
			memcpy(&fileattr, &payload->command_struct.setattr_cmd,
				sizeof(sstack_file_attribute_t));
			setattrcmd.attribute = &fileattr;
			cmd.setattr_cmd = &setattrcmd;
			msg.command_struct = &cmd;
			len = sstack_payload_t__get_packed_size(&msg);
			hdr.payload_len = len - sizeof(sstack_payload_hdr_t);
			msg.hdr = &hdr; // Parannoid 
			buffer = malloc(len);
			if (NULL == buffer) {
				sfs_log(ctx, SFS_ERR, "%s: Unable to allocate memory for "
					"%s. Command aborted \n", __FUNCTION__,
					sstack_command_stringify(payload->command));
	
				return -ENOMEM;
			}
			sstack_payload_t__pack(&msg, buffer);
			ret = _send_payload(transport, handle, buffer, len);
			if (ret != 0) {
				sfs_log(ctx, SFS_ERR, "%s: Failed to xmit payload "
					"for %s request. Error = %d \n",
					 __FUNCTION__, 
					sstack_command_stringify(payload->command), ret);

				free(buffer);

				return -ret;
			} else {
				sfs_log(ctx, SFS_INFO, "%s: Successfully xmitted payload "
					"for %s request \n",
					 __FUNCTION__,
					sstack_command_stringify(payload->command));

				free(buffer);

				return 0;
			}
		}
		case NFS_LOOKUP: {
			SstackNfsLookupCmd lookupcmd = SSTACK_NFS_LOOKUP_CMD__INIT;
			SstackFileNameT where = SSTACK_FILE_NAME_T__INIT;
			SstackFileNameT what = SSTACK_FILE_NAME_T__INIT;
			size_t len = 0;
			char *buffer = NULL;


			sfs_log(ctx, SFS_INFO, "%s: %s called \n", __FUNCTION__,
					sstack_command_stringify(payload->command));
			msg.command = SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__NFS_LOOKUP;
			where.name_len = payload->command_struct.lookup_cmd.where.name_len;
			where.name.len = where.name_len;
			strncpy((char *)&where.name.data,
				(char *) payload->command_struct.lookup_cmd.where.name,
				where.name.len );
			what.name_len = payload->command_struct.lookup_cmd.what.name_len;
			what.name.len = what.name_len;
			strncpy((char *)&what.name.data,
				(char *) payload->command_struct.lookup_cmd.what.name,
				what.name.len );
			lookupcmd.where = &where;
			lookupcmd.what = &what;
			cmd.lookup_cmd = &lookupcmd;
			msg.command_struct = &cmd;
			len = sstack_payload_t__get_packed_size(&msg);
			hdr.payload_len = len - sizeof(sstack_payload_hdr_t);
			msg.hdr = &hdr; // Parannoid 
			buffer = malloc(len);
			if (NULL == buffer) {
				sfs_log(ctx, SFS_ERR, "%s: Unable to allocate memory for "
					"%s. Command aborted \n", __FUNCTION__,
					sstack_command_stringify(payload->command));
	
				return -ENOMEM;
			}
			sstack_payload_t__pack(&msg, buffer);
			ret = _send_payload(transport, handle, buffer, len);
			if (ret != 0) {
				sfs_log(ctx, SFS_ERR, "%s: Failed to xmit payload "
					"for %s request. Error = %d \n",
					 __FUNCTION__, 
					sstack_command_stringify(payload->command), ret);

				free(buffer);

				return -ret;
			} else {
				sfs_log(ctx, SFS_INFO, "%s: Successfully xmitted payload "
					"for %s request \n",
					 __FUNCTION__,
					sstack_command_stringify(payload->command));

				free(buffer);

				return 0;
			}
		}

		case NFS_ACCESS: {
			SstackNfsAccessCmd accesscmd = SSTACK_NFS_ACCESS_CMD__INIT;
			size_t len;
			char *buffer = NULL;

			sfs_log(ctx, SFS_INFO, "%s: %s called \n", __FUNCTION__,
					sstack_command_stringify(payload->command));
			msg.command = SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__NFS_ACCESS;
			accesscmd.mode = payload->command_struct.access_cmd.mode;
			cmd.access_cmd = &accesscmd;
			msg.command_struct = &cmd;
			len = sstack_payload_t__get_packed_size(&msg);
			hdr.payload_len = len - sizeof(sstack_payload_hdr_t);
			msg.hdr = &hdr; // Parannoid 
			buffer = malloc(len);
			if (NULL == buffer) {
				sfs_log(ctx, SFS_ERR, "%s: Unable to allocate memory for "
					"%s. Command aborted \n", __FUNCTION__,
					sstack_command_stringify(payload->command));
	
				return -ENOMEM;
			}
			sstack_payload_t__pack(&msg, buffer);
			ret = _send_payload(transport, handle, buffer, len);
			if (ret != 0) {
				sfs_log(ctx, SFS_ERR, "%s: Failed to xmit payload "
					"for %s request. Error = %d \n",
					 __FUNCTION__, 
					sstack_command_stringify(payload->command), ret);

				free(buffer);

				return -ret;
			} else {
				sfs_log(ctx, SFS_INFO, "%s: Successfully xmitted payload "
					"for %s request \n",
					 __FUNCTION__,
					sstack_command_stringify(payload->command));

				free(buffer);

				return 0;
			}
		}
		case NFS_READ: {
			SstackNfsReadCmd readcmd = SSTACK_NFS_READ_CMD__INIT;
			size_t len;
			char *buffer = NULL;

			sfs_log(ctx, SFS_INFO, "%s: %s called \n", __FUNCTION__,
					sstack_command_stringify(payload->command));
			msg.command = SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__NFS_READ;
			readcmd.offset = payload->command_struct.read_cmd.offset;
			readcmd.count = payload->command_struct.read_cmd.count;
			cmd.read_cmd = &readcmd;
			msg.command_struct = &cmd;
			len = sstack_payload_t__get_packed_size(&msg);
			hdr.payload_len = len - sizeof(sstack_payload_hdr_t);
			msg.hdr = &hdr; // Parannoid 
			buffer = malloc(len);
			if (NULL == buffer) {
				sfs_log(ctx, SFS_ERR, "%s: Unable to allocate memory for "
					"%s. Command aborted \n", __FUNCTION__,
					sstack_command_stringify(payload->command));
	
				return -ENOMEM;
			}
			sstack_payload_t__pack(&msg, buffer);
			ret = _send_payload(transport, handle, buffer, len);
			if (ret != 0) {
				sfs_log(ctx, SFS_ERR, "%s: Failed to xmit payload "
					"for %s request. Error = %d \n",
					 __FUNCTION__, 
					sstack_command_stringify(payload->command), ret);

				free(buffer);

				return -ret;
			} else {
				sfs_log(ctx, SFS_INFO, "%s: Successfully xmitted payload "
					"for %s request \n",
					 __FUNCTION__,
					sstack_command_stringify(payload->command));

				free(buffer);

				return 0;
			}
		}
		case NFS_WRITE: {
			SstackNfsWriteCmd writecmd = SSTACK_NFS_WRITE_CMD__INIT;
			SstackNfsData data = SSTACK_NFS_DATA__INIT;
			size_t len;
			char *buffer = NULL;

			sfs_log(ctx, SFS_INFO, "%s: %s called \n", __FUNCTION__,
					sstack_command_stringify(payload->command));
			msg.command = SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__NFS_WRITE;
			data.data_len = payload->command_struct.write_cmd.data.data_len;
			data.data_val.len = data.data_len;
			strncpy((char *) &data.data_val.data,
				(char *) &payload->command_struct.write_cmd.data.data_val,
				data.data_len);
			writecmd.offset = payload->command_struct.write_cmd.offset;
			writecmd.count = payload->command_struct.write_cmd.count;
			writecmd.data = &data;
			cmd.write_cmd = &writecmd;
			msg.command_struct = &cmd;
			len = sstack_payload_t__get_packed_size(&msg);
			hdr.payload_len = len - sizeof(sstack_payload_hdr_t);
			msg.hdr = &hdr; // Parannoid 
			buffer = malloc(len);
			if (NULL == buffer) {
				sfs_log(ctx, SFS_ERR, "%s: Unable to allocate memory for "
					"%s. Command aborted \n", __FUNCTION__,
					sstack_command_stringify(payload->command));
	
				return -ENOMEM;
			}
			sstack_payload_t__pack(&msg, buffer);
			ret = _send_payload(transport, handle, buffer, len);
			if (ret != 0) {
				sfs_log(ctx, SFS_ERR, "%s: Failed to xmit payload "
					"for %s request. Error = %d \n",
					 __FUNCTION__, 
					sstack_command_stringify(payload->command), ret);

				free(buffer);

				return -ret;
			} else {
				sfs_log(ctx, SFS_INFO, "%s: Successfully xmitted payload "
					"for %s request \n",
					 __FUNCTION__,
					sstack_command_stringify(payload->command));

				free(buffer);

				return 0;
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