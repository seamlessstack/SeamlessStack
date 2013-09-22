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
 * _sendrecv_payload - Helper function to call transport function
 * Main itention is to avoid clutter for each command handling.
 *
 * transport - transport structure used between sfs and sfsd
 * handle - client handle. Should be non-NULL.
 * buffer - preallocated buffer containing packed payload
 * len - Length of the packed payload.
 * tx - boolean to indicate tx or rx. If 1 then tx. Else rx
 *
 * Return 0 on success for xmit and number bytes received for recv.
 * Returns a negative number indicating error on failure.
 */ 

static inline int
_sendrecv_payload(sstack_transport_t *transport,
			sstack_client_handle_t handle,
			char *buffer,
			size_t len,
			int tx)
{

	if (transport && transport->transport_ops.tx &&
			transport->transport_ops.rx  && (handle > 0)) {
		int ret = -1;

		if (tx == 1)
			ret = transport->transport_ops.tx(handle, len, (void *) buffer);
		else if (tx == 0)
			ret = transport->transport_ops.rx(handle, len, (void *) buffer);
		if (ret == -1) {
			// Request failed
			return -errno;
		}
		// Assumption is tx will try few times to transmit full data.
		// If it is unsuccessul, tx returns -1.
		// So not checking for partial data transfer here.
		// For recv, header is received first followed by the real payload.
		// recv will keep trying till number of bytes mentioned in the 
		// payload heder are received. So no partial recvs are possible.
		if (tx == 1)
			return 0;
		else if (tx == 0)
			return ret;
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
	SstackNfsResponseStruct response = SSTACK_NFS_RESPONSE_STRUCT__INIT;
	uint32_t payload_len = 0;
	int ret = -1;
	size_t len = 0;
	char *buffer = NULL;

	hdr.sequence = sequence++;
	hdr.payload_len = 0; // Dummy . will be filled later
	msg.hdr = &hdr;
	switch (payload->command) {
		case SSTACK_ADD_STORAGE:
		case SSTACK_REMOVE_STORAGE:
		case SSTACK_UPDATE_STORAGE: {
			SfsdStorageT storage = SFSD_STORAGE_T__INIT;

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
			ret = _sendrecv_payload(transport, handle, buffer, len, 1);
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
			ret = _sendrecv_payload(transport, handle, buffer, len, 1);
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
			ret = _sendrecv_payload(transport, handle, buffer, len, 1);
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
			ret = _sendrecv_payload(transport, handle, buffer, len, 1);
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
			PolicyEntry entry = POLICY_ENTRY__INIT;
			Attribute attr = ATTRIBUTE__INIT;
			PolicyPlugin **plugins = NULL;
			int i = 0;

			sfs_log(ctx, SFS_INFO, "%s: %s called \n", __FUNCTION__,
					sstack_command_stringify(payload->command));
			msg.command = SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__NFS_READ;
			readcmd.offset = payload->command_struct.read_cmd.offset;
			readcmd.count = payload->command_struct.read_cmd.count;
			attr.ver = payload->command_struct.read_cmd.entry.pe_attr.ver;
			attr.a_qoslevel =
				payload->command_struct.read_cmd.entry.pe_attr.a_qoslevel;
			attr.a_ishidden =
				payload->command_struct.read_cmd.entry.pe_attr.a_ishidden;
			entry.pe_attr = &attr;
			entry.pe_num_plugins =
				payload->command_struct.read_cmd.entry.pe_num_plugins;
			entry.pe_refcount =
				payload->command_struct.read_cmd.entry.pe_refcount;
			entry.pe_lock =
				payload->command_struct.read_cmd.entry.pe_lock;
			entry.pst_index =
				payload->command_struct.read_cmd.entry.pst_index;
			plugins = (PolicyPlugin **) malloc(entry.pe_num_plugins *
					sizeof(PolicyPlugin));
			if (NULL == plugins) {
				sfs_log(ctx, SFS_ERR, "%s: Unable to allocate memory for "
					"%s. Command aborted \n", __FUNCTION__,
					sstack_command_stringify(payload->command));
	
				return -ENOMEM;
			}
			for ( i = 0; i < entry.pe_num_plugins; i++ ) {
				plugins[i]->ver =
					payload->command_struct.read_cmd.entry.pe_policy[i]->ver;	
				plugins[i]->pp_refcount =
					payload->command_struct.read_cmd.entry.pe_policy[i]
						->pp_refcount;
				plugins[i]->pp_policy_name.len =
					strlen(payload->command_struct.read_cmd.entry.pe_policy[i]
						->pp_policy_name);
				strcpy((char *)&plugins[i]->pp_policy_name.data,
					payload->command_struct.read_cmd.entry.pe_policy[i]
						->pp_policy_name);	
				plugins[i]->pp_sha_sum.len =
					strlen(payload->command_struct.read_cmd.entry.pe_policy[i]
						->pp_sha_sum);
				strcpy((char *) &plugins[i]->pp_sha_sum.data,
					payload->command_struct.read_cmd.entry.pe_policy[i]
						->pp_sha_sum);	
				plugins[i]->is_activated =
					payload->command_struct.read_cmd.entry.pe_policy[i]
						->is_activated;
				plugins[i]->pp_lock =
					payload->command_struct.read_cmd.entry.pe_policy[i]
						->pp_lock;
			}
			entry.pe_policy = plugins;
			readcmd.entry = &entry;
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
			ret = _sendrecv_payload(transport, handle, buffer, len, 1);
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
			PolicyEntry entry = POLICY_ENTRY__INIT;
			Attribute attr = ATTRIBUTE__INIT;
			PolicyPlugin **plugins = NULL;
			int i = 0;

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
			attr.ver = payload->command_struct.write_cmd.entry.pe_attr.ver;
			attr.a_qoslevel =
				payload->command_struct.write_cmd.entry.pe_attr.a_qoslevel;
			attr.a_ishidden =
				payload->command_struct.write_cmd.entry.pe_attr.a_ishidden;
			entry.pe_attr = &attr;
			entry.pe_num_plugins =
				payload->command_struct.write_cmd.entry.pe_num_plugins;
			entry.pe_refcount =
				payload->command_struct.write_cmd.entry.pe_refcount;
			entry.pe_lock =
				payload->command_struct.write_cmd.entry.pe_lock;
			entry.pst_index =
				payload->command_struct.write_cmd.entry.pst_index;
			plugins = (PolicyPlugin **) malloc(entry.pe_num_plugins *
					sizeof(PolicyPlugin));
			if (NULL == plugins) {
				sfs_log(ctx, SFS_ERR, "%s: Unable to allocate memory for "
					"%s. Command aborted \n", __FUNCTION__,
					sstack_command_stringify(payload->command));
	
				return -ENOMEM;
			}
			for ( i = 0; i < entry.pe_num_plugins; i++ ) {
				plugins[i]->ver =
					payload->command_struct.write_cmd.entry.pe_policy[i]->ver;	
				plugins[i]->pp_refcount =
					payload->command_struct.write_cmd.entry.pe_policy[i]
						->pp_refcount;
				plugins[i]->pp_policy_name.len =
					strlen(payload->command_struct.write_cmd.entry.pe_policy[i]
						->pp_policy_name);
				strcpy((char *)&plugins[i]->pp_policy_name.data,
					payload->command_struct.write_cmd.entry.pe_policy[i]
						->pp_policy_name);	
				plugins[i]->pp_sha_sum.len =
					strlen(payload->command_struct.write_cmd.entry.pe_policy[i]
						->pp_sha_sum);
				strcpy((char *) &plugins[i]->pp_sha_sum.data,
					payload->command_struct.write_cmd.entry.pe_policy[i]
						->pp_sha_sum);	
				plugins[i]->is_activated =
					payload->command_struct.write_cmd.entry.pe_policy[i]
						->is_activated;
				plugins[i]->pp_lock =
					payload->command_struct.write_cmd.entry.pe_policy[i]
						->pp_lock;
			}
			entry.pe_policy = plugins;
			writecmd.entry = &entry;
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
			ret = _sendrecv_payload(transport, handle, buffer, len, 1);
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
		case NFS_CREATE: {
			SstackNfsCreateCmd createcmd = SSTACK_NFS_CREATE_CMD__INIT;
			SstackNfsData data = SSTACK_NFS_DATA__INIT;
			PolicyEntry entry = POLICY_ENTRY__INIT;
			Attribute attr = ATTRIBUTE__INIT;
			PolicyPlugin **plugins = NULL;
			int i = 0;

			sfs_log(ctx, SFS_INFO, "%s: %s called \n", __FUNCTION__,
					sstack_command_stringify(payload->command));
			msg.command = SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__NFS_CREATE;
			data.data_len = payload->command_struct.create_cmd.data.data_len;
			data.data_val.len = data.data_len;
			strncpy((char *) &data.data_val.data,
				(char *) &payload->command_struct.create_cmd.data.data_val,
				data.data_len);
			createcmd.mode = payload->command_struct.create_cmd.mode;
			createcmd.data = &data;
			attr.ver = payload->command_struct.create_cmd.entry.pe_attr.ver;
			attr.a_qoslevel =
				payload->command_struct.create_cmd.entry.pe_attr.a_qoslevel;
			attr.a_ishidden =
				payload->command_struct.create_cmd.entry.pe_attr.a_ishidden;
			entry.pe_attr = &attr;
			entry.pe_num_plugins =
				payload->command_struct.create_cmd.entry.pe_num_plugins;
			entry.pe_refcount =
				payload->command_struct.create_cmd.entry.pe_refcount;
			entry.pe_lock =
				payload->command_struct.create_cmd.entry.pe_lock;
			entry.pst_index =
				payload->command_struct.create_cmd.entry.pst_index;
			plugins = (PolicyPlugin **) malloc(entry.pe_num_plugins *
					sizeof(PolicyPlugin));
			if (NULL == plugins) {
				sfs_log(ctx, SFS_ERR, "%s: Unable to allocate memory for "
					"%s. Command aborted \n", __FUNCTION__,
					sstack_command_stringify(payload->command));
	
				return -ENOMEM;
			}
			for ( i = 0; i < entry.pe_num_plugins; i++ ) {
				plugins[i]->ver =
					payload->command_struct.create_cmd.entry.pe_policy[i]->ver;	
				plugins[i]->pp_refcount =
					payload->command_struct.create_cmd.entry.pe_policy[i]
						->pp_refcount;
				plugins[i]->pp_policy_name.len =
					strlen(payload->command_struct.create_cmd.entry.pe_policy[i]
						->pp_policy_name);
				strcpy((char *)&plugins[i]->pp_policy_name.data,
					payload->command_struct.create_cmd.entry.pe_policy[i]
						->pp_policy_name);	
				plugins[i]->pp_sha_sum.len =
					strlen(payload->command_struct.create_cmd.entry.pe_policy[i]
						->pp_sha_sum);
				strcpy((char *) &plugins[i]->pp_sha_sum.data,
					payload->command_struct.create_cmd.entry.pe_policy[i]
						->pp_sha_sum);	
				plugins[i]->is_activated =
					payload->command_struct.create_cmd.entry.pe_policy[i]
						->is_activated;
				plugins[i]->pp_lock =
					payload->command_struct.create_cmd.entry.pe_policy[i]
						->pp_lock;
			}
			entry.pe_policy = plugins;
			createcmd.entry = &entry;
			cmd.create_cmd = &createcmd;
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
			ret = _sendrecv_payload(transport, handle, buffer, len, 1);
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
		case NFS_MKDIR: {
			SstackNfsMkdirCmd mkdircmd = SSTACK_NFS_MKDIR_CMD__INIT;

			sfs_log(ctx, SFS_INFO, "%s: %s called \n", __FUNCTION__,
					sstack_command_stringify(payload->command));
			msg.command = SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__NFS_MKDIR;
			mkdircmd.mode = payload->command_struct.mkdir_cmd.mode;
			cmd.mkdir_cmd = &mkdircmd;
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
			ret = _sendrecv_payload(transport, handle, buffer, len, 1);
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
		case NFS_SYMLINK: {
			SstackNfsSymlinkCmd symlinkcmd = SSTACK_NFS_SYMLINK_CMD__INIT;
			SstackFileNameT new_path = SSTACK_FILE_NAME_T__INIT;

			sfs_log(ctx, SFS_INFO, "%s: %s called \n", __FUNCTION__,
					sstack_command_stringify(payload->command));
			msg.command = SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__NFS_SYMLINK;
			new_path.name_len =
				payload->command_struct.symlink_cmd.new_path.name_len;
			new_path.name.len = new_path.name_len;
			strncpy((char *)&new_path.name.data,
				(char *) payload->command_struct.symlink_cmd.new_path.name,
				new_path.name.len );
			symlinkcmd.new_path = &new_path;
			cmd.symlink_cmd = &symlinkcmd;
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
			ret = _sendrecv_payload(transport, handle, buffer, len, 1);
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
		case NFS_RENAME: {
			SstackNfsRenameCmd renamecmd = SSTACK_NFS_RENAME_CMD__INIT;
			SstackFileNameT new_path = SSTACK_FILE_NAME_T__INIT;

			sfs_log(ctx, SFS_INFO, "%s: %s called \n", __FUNCTION__,
					sstack_command_stringify(payload->command));
			msg.command = SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__NFS_RENAME;
			new_path.name_len =
				payload->command_struct.rename_cmd.new_path.name_len;
			new_path.name.len = new_path.name_len;
			strncpy((char *)&new_path.name.data,
				(char *) payload->command_struct.rename_cmd.new_path.name,
				new_path.name.len);
			renamecmd.new_path = &new_path;
			cmd.rename_cmd = &renamecmd;
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
			ret = _sendrecv_payload(transport, handle, buffer, len, 1);
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
		case NFS_REMOVE: {
			SstackNfsRemoveCmd removecmd = SSTACK_NFS_REMOVE_CMD__INIT;

			sfs_log(ctx, SFS_INFO, "%s: %s called \n", __FUNCTION__,
					sstack_command_stringify(payload->command));
			msg.command = SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__NFS_REMOVE;
			removecmd.path_len =
				payload->command_struct.remove_cmd.path_len;
			removecmd.path.len = removecmd.path_len;
			strncpy((char *) &removecmd.path.data,
				(char *) &payload->command_struct.remove_cmd.path,
				removecmd.path.len);
			cmd.remove_cmd = &removecmd;
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
			ret = _sendrecv_payload(transport, handle, buffer, len, 1);
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
		case NFS_COMMIT: {
			SstackNfsCommitCmd commitcmd = SSTACK_NFS_COMMIT_CMD__INIT;

			sfs_log(ctx, SFS_INFO, "%s: %s called \n", __FUNCTION__,
					sstack_command_stringify(payload->command));
			msg.command = SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__NFS_COMMIT;
			commitcmd.offset = payload->command_struct.commit_cmd.offset;
			commitcmd.count = payload->command_struct.commit_cmd.count;
			cmd.commit_cmd = &commitcmd;
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
			ret = _sendrecv_payload(transport, handle, buffer, len, 1);
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
		case SSTACK_REMOVE_STORAGE_RSP:
		case SSTACK_UPDATE_STORAGE_RSP:
		case SSTACK_ADD_STORAGE_RSP:
			response.command_ok = payload->response_struct.command_ok;
			msg.response_struct = &response;
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
			ret = _sendrecv_payload(transport, handle, buffer, len, 1);
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
		case NFS_GETATTR_RSP: {
			SstackNfsGetattrResp getattr_resp = SSTACK_NFS_GETATTR_RESP__INIT;
			Stat stat = STAT__INIT;

			memcpy((void *) &stat.st_dev,
					(void *)&payload->response_struct.getattr_resp.stbuf,
					sizeof(struct stat));
			getattr_resp.stbuf = &stat;
			response.getattr_resp = &getattr_resp;
			msg.response_struct = &response;
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
			ret = _sendrecv_payload(transport, handle, buffer, len, 1);
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
		case NFS_LOOKUP_RSP: {
			SstackNfsLookupResp lookup_resp = SSTACK_NFS_LOOKUP_RESP__INIT;

			lookup_resp.lookup_path_len =
					payload->response_struct.lookup_resp.lookup_path_len;
			lookup_resp.lookup_path.len =
					strlen(payload->response_struct.lookup_resp.lookup_path);
			strcpy((char *) &lookup_resp.lookup_path.data,
							payload->response_struct.lookup_resp.lookup_path);
			response.lookup_resp = &lookup_resp;
			msg.response_struct = &response;
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
			ret = _sendrecv_payload(transport, handle, buffer, len, 1);
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
		case NFS_ACCESS_RSP: {
			SstackNfsAccessResp access_resp = SSTACK_NFS_ACCESS_RESP__INIT;

			access_resp.access = payload->response_struct.access_resp.access;
			response.access_resp = &access_resp;
			msg.response_struct = &response;
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
			ret = _sendrecv_payload(transport, handle, buffer, len, 1);
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
		case NFS_READ_RSP: {
			SstackNfsReadResp read_resp = SSTACK_NFS_READ_RESP__INIT;
			SstackNfsData data = SSTACK_NFS_DATA__INIT;

			read_resp.count = payload->response_struct.read_resp.count;
			read_resp.eof = payload->response_struct.read_resp.eof;
			data.data_len = payload->response_struct.read_resp.data.data_len;
			data.data_val.len = data.data_len;
			strcpy((char *) &data.data_val.data,
							payload->response_struct.read_resp.data.data_val);
			read_resp.data = &data;
			response.read_resp = &read_resp;
			msg.response_struct = &response;
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
			ret = _sendrecv_payload(transport, handle, buffer, len, 1);
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
		case NFS_WRITE_RSP: {
			SstackNfsWriteResp write_resp = SSTACK_NFS_WRITE_RESP__INIT;

			write_resp.file_create_ok =
					payload->response_struct.write_resp.file_create_ok;
			write_resp.file_wc = payload->response_struct.write_resp.file_wc;
			response.write_resp = &write_resp;
			msg.response_struct = &response;
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
			ret = _sendrecv_payload(transport, handle, buffer, len, 1);
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
		case NFS_READLINK_RSP: {
			SstackNfsReadlinkResp readlink_resp =
					SSTACK_NFS_READLINK_RESP__INIT;
			SstackFileNameT real_file = SSTACK_FILE_NAME_T__INIT;

			real_file.name_len =
					payload->response_struct.readlink_resp.real_file.name_len;
			real_file.name.len =
				strlen(payload->response_struct.readlink_resp.real_file.name);
			strcpy((char *) &real_file.name.data,
				payload->response_struct.readlink_resp.real_file.name);
			readlink_resp.real_file = &real_file;
			response.readlink_resp = &readlink_resp;
			msg.response_struct = &response;
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
			ret = _sendrecv_payload(transport, handle, buffer, len, 1);
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
		case NFS_CREATE_RSP: {
			SstackNfsWriteResp create_resp = SSTACK_NFS_WRITE_RESP__INIT;

			create_resp.file_create_ok =
					payload->response_struct.create_resp.file_create_ok;
			create_resp.file_wc = payload->response_struct.create_resp.file_wc;
			response.create_resp = &create_resp;
			msg.response_struct = &response;
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
			ret = _sendrecv_payload(transport, handle, buffer, len, 1);
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
	SstackPayloadT *msg = NULL;
	ssize_t payload_len = 0;
	sstack_payload_t *payload = NULL;
	char temp_payload[sizeof(sstack_payload_t)]; // Max size of payload


	payload = (sstack_payload_t *) malloc(sizeof(sstack_payload_t));
	if (NULL == payload) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to allocate memory for receiving"
			" payload. Client handle = %"PRId64" \n",
			__FUNCTION__, handle);
		return NULL;
	}

	memset((void *)payload, '\0', sizeof(sstack_payload_t));
	// Call the transport function to receive the buffer
	payload_len = _sendrecv_payload(transport, handle, temp_payload,
			 (size_t) sizeof(sstack_payload_t), 0);
	if (payload_len == -1) {
		sfs_log(ctx, SFS_ERR, "%s: Recv failed for payload. "
			"Client handle = %"PRId64" \n",
			__FUNCTION__, handle);
		return NULL;
	}
	// Parse temp_payload and populate payload
	msg = sstack_payload_t__unpack(NULL, payload_len, temp_payload);
	// Populate the payload structure with required fields
	payload->hdr.sequence = msg->hdr->sequence;
	payload->hdr.payload_len = msg->hdr->payload_len;
	payload->command = msg->command;

	switch(msg->command) {
		case SSTACK_ADD_STORAGE:
		case SSTACK_REMOVE_STORAGE:
		case SSTACK_UPDATE_STORAGE: {
			ProtobufCBinaryData data = msg->storage->path;

			strcpy(payload->storage.path, (char *) &data.data);
			data = msg->storage->mount_point;
			strcpy(payload->storage.mount_point, (char *) &data.data);
			payload->storage.weight = msg->storage->weight;
			payload->storage.nblocks = msg->storage->nblocks;
			payload->storage.protocol = msg->storage->protocol;
			if (transport->transport_hdr.tcp.ipv4) {
				data = msg->storage->ipv4_addr;
				strcpy(payload->storage.ipv4_addr, (char *) &data.data);
			} else {
				data = msg->storage->ipv6_addr;
				strcpy(payload->storage.ipv6_addr, (char *) &data.data);
			}

			return payload;
		}
		case SSTACK_ADD_STORAGE_RSP:
		case SSTACK_REMOVE_STORAGE_RSP:
		case SSTACK_UPDATE_STORAGE_RSP: {
			payload->response_struct.command_ok =
				msg->response_struct->command_ok;
			if (payload->response_struct.command_ok == 1) {
				sfs_log(ctx, SFS_INFO, "%s: Command %s succeeded. "
					"Client handle = %"PRId64" \n",
					__FUNCTION__,
					sstack_command_stringify(msg->command),
					handle);
			}
			return payload;
		}
		case NFS_SETATTR: {
			SstackFileAttributeT *attr =
					msg->command_struct->setattr_cmd->attribute;

			memcpy( (void *) &payload->command_struct.setattr_cmd.attribute,
							(void *) &attr->mode, sizeof(struct attribute));
			return payload;
		}
		case NFS_LOOKUP: {
			SstackFileNameT *filename = NULL;
			ProtobufCBinaryData data;

			payload->command_struct.lookup_cmd.where.name_len =
					msg->command_struct->lookup_cmd->where->name_len;
			data = msg->command_struct->lookup_cmd->where->name;
			strcpy(payload->command_struct.lookup_cmd.where.name, (char *)
							&data.data);
			data = msg->command_struct->lookup_cmd->what->name;
			strcpy(payload->command_struct.lookup_cmd.what.name, (char *)
							&data.data);
			return payload;
		}
		case NFS_ACCESS:
			payload->command_struct.access_cmd.mode =
					msg->command_struct->access_cmd->mode;

			return payload;
		case NFS_READ: {
			PolicyEntry *entry = NULL;
			PolicyPlugin **plugins = NULL;
			Attribute *attr = NULL;
			int i = 0;

			payload->command_struct.read_cmd.offset =
					msg->command_struct->read_cmd->offset;
			payload->command_struct.read_cmd.count =
					msg->command_struct->read_cmd->count;
			entry = msg->command_struct->read_cmd->entry;
			attr = entry->pe_attr;
			memcpy( (void *) &payload->command_struct.read_cmd.entry.pe_attr,
							(void *) &attr->ver, sizeof(struct attribute));
			payload->command_struct.read_cmd.entry.pe_num_plugins =
					entry->pe_num_plugins;
			payload->command_struct.read_cmd.entry.pe_refcount =
					entry->pe_refcount;
			payload->command_struct.read_cmd.entry.pe_lock =
					entry->pe_lock;
			payload->command_struct.read_cmd.entry.pst_index =
					entry->pst_index;
			plugins = msg->command_struct->read_cmd->entry->pe_policy;
			for ( i = 0; i < entry->pe_num_plugins; i++ ) {
				payload->command_struct.read_cmd.entry.pe_policy[i]->ver =
						plugins[i]->ver;
				payload->command_struct.read_cmd.entry.
					pe_policy[i]->pp_refcount = plugins[i]->pp_refcount;
				strcpy(payload->command_struct.read_cmd.entry.
					pe_policy[i]->pp_policy_name,
					(char*) &plugins[i]->pp_policy_name.data);
				strcpy(payload->command_struct.read_cmd.entry.
					pe_policy[i]->pp_sha_sum,
					(char *) &plugins[i]->pp_sha_sum.data);

				payload->command_struct.read_cmd.entry.
					pe_policy[i]->is_activated = plugins[i]->is_activated;
				payload->command_struct.read_cmd.entry.pe_policy[i]->pp_lock =
					plugins[i]->pp_lock;
			}

			return payload;
		}
		case NFS_WRITE: {
			PolicyEntry *entry = NULL;
			PolicyPlugin **plugins = NULL;
			Attribute *attr = NULL;
			SstackNfsData *data = NULL;
			ProtobufCBinaryData data1;
			int i = 0;

			payload->command_struct.write_cmd.offset =
					msg->command_struct->write_cmd->offset;
			payload->command_struct.write_cmd.count =
					msg->command_struct->write_cmd->count;
			data = msg->command_struct->write_cmd->data;
			payload->command_struct.write_cmd.data.data_len = data->data_len;
			data1 = msg->command_struct->write_cmd->data->data_val;
			strcpy((char *) &payload->command_struct.write_cmd.data.data_val,
							(char *) &data1.data);

			entry = msg->command_struct->write_cmd->entry;
			attr = entry->pe_attr;
			memcpy( (void *) &payload->command_struct.write_cmd.entry.pe_attr,
							(void *) &attr->ver, sizeof(struct attribute));
			payload->command_struct.write_cmd.entry.pe_num_plugins =
					entry->pe_num_plugins;
			payload->command_struct.write_cmd.entry.pe_refcount =
					entry->pe_refcount;
			payload->command_struct.write_cmd.entry.pe_lock =
					entry->pe_lock;
			payload->command_struct.write_cmd.entry.pst_index =
					entry->pst_index;
			plugins = msg->command_struct->write_cmd->entry->pe_policy;
			for ( i = 0; i < entry->pe_num_plugins; i++ ) {
				payload->command_struct.write_cmd.entry.pe_policy[i]->ver =
						plugins[i]->ver;
				payload->command_struct.write_cmd.entry.
					pe_policy[i]->pp_refcount = plugins[i]->pp_refcount;
				strcpy(payload->command_struct.write_cmd.entry.
					pe_policy[i]->pp_policy_name,
					(char*) &plugins[i]->pp_policy_name.data);
				strcpy(payload->command_struct.write_cmd.entry.
					pe_policy[i]->pp_sha_sum,
					(char *) &plugins[i]->pp_sha_sum.data);

				payload->command_struct.write_cmd.entry.
					pe_policy[i]->is_activated = plugins[i]->is_activated;
				payload->command_struct.write_cmd.entry.pe_policy[i]->pp_lock =
					plugins[i]->pp_lock;
			}

			return payload;
		}
		case NFS_CREATE: {
			PolicyEntry *entry = NULL;
			PolicyPlugin **plugins = NULL;
			Attribute *attr = NULL;
			SstackNfsData *data = NULL;
			ProtobufCBinaryData data1;
			int i = 0;

			payload->command_struct.create_cmd.mode =
					msg->command_struct->create_cmd->mode;
			data = msg->command_struct->create_cmd->data;
			payload->command_struct.create_cmd.data.data_len = data->data_len;
			data1 = msg->command_struct->create_cmd->data->data_val;
			strcpy((char *) &payload->command_struct.create_cmd.data.data_val,
							(char *) &data1.data);

			entry = msg->command_struct->create_cmd->entry;
			attr = entry->pe_attr;
			memcpy( (void *) &payload->command_struct.create_cmd.entry.pe_attr,
							(void *) &attr->ver, sizeof(struct attribute));
			payload->command_struct.create_cmd.entry.pe_num_plugins =
					entry->pe_num_plugins;
			payload->command_struct.create_cmd.entry.pe_refcount =
					entry->pe_refcount;
			payload->command_struct.create_cmd.entry.pe_lock =
					entry->pe_lock;
			payload->command_struct.create_cmd.entry.pst_index =
					entry->pst_index;
			plugins = msg->command_struct->create_cmd->entry->pe_policy;
			for ( i = 0; i < entry->pe_num_plugins; i++ ) {
				payload->command_struct.create_cmd.entry.pe_policy[i]->ver =
						plugins[i]->ver;
				payload->command_struct.create_cmd.entry.
					pe_policy[i]->pp_refcount = plugins[i]->pp_refcount;
				strcpy(payload->command_struct.create_cmd.entry.
					pe_policy[i]->pp_policy_name,
					(char*) &plugins[i]->pp_policy_name.data);
				strcpy(payload->command_struct.create_cmd.entry.
					pe_policy[i]->pp_sha_sum,
					(char *) &plugins[i]->pp_sha_sum.data);

				payload->command_struct.create_cmd.entry.
					pe_policy[i]->is_activated = plugins[i]->is_activated;
				payload->command_struct.create_cmd.entry.pe_policy[i]->pp_lock =
					plugins[i]->pp_lock;
			}

			return payload;
		}
		case NFS_MKDIR:
			payload->command_struct.mkdir_cmd.mode =
					msg->command_struct->mkdir_cmd->mode;

			return payload;

		case NFS_SYMLINK: {
			SstackFileNameT *filename = NULL;
			ProtobufCBinaryData data;

			payload->command_struct.symlink_cmd.new_path.name_len =
					msg->command_struct->symlink_cmd->new_path->name_len;
			data = msg->command_struct->symlink_cmd->new_path->name;
			strcpy(payload->command_struct.symlink_cmd.new_path.name, (char *)
							&data.data);
			return payload;
		}
		case NFS_RENAME: {
			SstackFileNameT *filename = NULL;
			ProtobufCBinaryData data;

			payload->command_struct.rename_cmd.new_path.name_len =
					msg->command_struct->rename_cmd->new_path->name_len;
			data = msg->command_struct->rename_cmd->new_path->name;
			strcpy(payload->command_struct.rename_cmd.new_path.name, (char *)
							&data.data);
			return payload;
		}

		case NFS_REMOVE: {
			ProtobufCBinaryData path;

			payload->command_struct.remove_cmd.path_len =
					msg->command_struct->remove_cmd->path_len;
			path = msg->command_struct->remove_cmd->path;
			strccpy((char *) &payload->command_struct.remove_cmd.path,
							(char *) &path.data);

			return payload;
		}

		case NFS_COMMIT:
			payload->command_struct.commit_cmd.offset =
					msg->command_struct->commit_cmd->offset;
			payload->command_struct.commit_cmd.count =
					msg->command_struct->commit_cmd->count;

			return payload;

		case NFS_GETATTR_RSP:
			memcpy((void *) &payload->response_struct.getattr_resp.stbuf,
					(void *) msg->response_struct->getattr_resp->stbuf,
					sizeof(struct stat));

			return payload;
		case NFS_LOOKUP_RSP: {
			ProtobufCBinaryData lookup_path;

			payload->response_struct.lookup_resp.lookup_path_len =
				msg->response_struct->lookup_resp->lookup_path_len;
			lookup_path = msg->response_struct->lookup_resp->lookup_path;
			memcpy((void *) payload->response_struct.lookup_resp.lookup_path,
				(void *) lookup_path.data, lookup_path.len);
			return payload;
		}
		case NFS_ACCESS_RSP:
			payload->response_struct.access_resp.access =
				msg->response_struct->access_resp->access;
			return payload;
		case NFS_READLINK_RSP: {
			ProtobufCBinaryData name;

			payload->response_struct.readlink_resp.real_file.name_len =
				msg->response_struct->readlink_resp->real_file->name_len;
			name = msg->response_struct->readlink_resp->real_file->name;
			memcpy((void *) payload->response_struct.readlink_resp.real_file.name,
				(void *) name.data, name.len);
			return payload;
		}
		case NFS_READ_RSP: {
			ProtobufCBinaryData data_val;

			payload->response_struct.read_resp.count =
				msg->response_struct->read_resp->count;
			payload->response_struct.read_resp.eof =
				msg->response_struct->read_resp->eof;
			payload->response_struct.read_resp.data.data_len =
				msg->response_struct->read_resp->data->data_len;
			data_val = msg->response_struct->read_resp->data->data_val;
			memcpy((void *) payload->response_struct.read_resp.data.data_val,
				(void *) data_val.data, data_val.len);
			return payload;
		}
		case NFS_WRITE_RSP:
			payload->response_struct.write_resp.file_create_ok =
				msg->response_struct->write_resp->file_create_ok;
			payload->response_struct.write_resp.file_wc =
				msg->response_struct->write_resp->file_wc;
			return payload;
		case NFS_CREATE_RSP:
			payload->response_struct.create_resp.file_create_ok =
				msg->response_struct->create_resp->file_create_ok;
			payload->response_struct.create_resp.file_wc =
				msg->response_struct->create_resp->file_wc;
			return payload;
			
	}
	return NULL;
}
