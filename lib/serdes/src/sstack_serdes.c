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
#include <string.h>
#include <stdbool.h>
#include <sys/errno.h>
#include <sstack_serdes.h>
#include <sstack_types.h>
#include <jobs.pb-c.h>
#include <bds_slab.h>
#include <sstack_log.h>

static uint32_t sequence = 1; // Sequence number for packets
static bds_cache_desc_t serdes_caches[SERDES_NUM_CACHES] = {0};
log_ctx_t *serdes_ctx = NULL;


/* Initialize the payload and data caches -
 * Allocations are required in sstack_recv_payload
 * The sfs and sfsd modules would use these caches
 * in all their allocations and deallocations
 */

//__attribute__((constructor))
int32_t sstack_serdes_init(bds_cache_desc_t **cache_array)
{
	bds_status_t status;
	int32_t log_ret = 0;

	/* Create a log ctx for serdes library */
	serdes_ctx = sfs_create_log_ctx();
	ASSERT((NULL != serdes_ctx), "serdes log create failed", 1, 1, 0);
	log_ret = sfs_log_init(serdes_ctx, SFS_DEBUG, "serdes");
	ASSERT((0 == log_ret), "serdes log_init failed", 1, 1, 0);

	 /*
	  * Logging  is enabled now. sstack_log_directory should have
	  * been populated by sfs or sfsd by now
	  */
	
	/* Create the payload cache first */
	status = bds_cache_create("payload-cache", sizeof(sstack_payload_t),
							  0, NULL, NULL,
							  &serdes_caches[SERDES_PAYLOAD_CACHE_IDX]);
	if (status != 0) {
		sfs_log(serdes_ctx, SFS_CRIT, "%s(): %dpayload-cache failed\n",
				__FUNCTION__, __LINE__);
		return -ENOMEM;
	}
	sfs_log(serdes_ctx, SFS_DEBUG, "%s(): %d %p- payload-cache created\n",
			__FUNCTION__, __LINE__, serdes_caches[SERDES_PAYLOAD_CACHE_IDX]);

	/* Create data cache of size 4K */
	status = bds_cache_create("data4k-cache", 4096,
							  0, NULL, NULL,
							  &serdes_caches[SERDES_DATA_4K_CACHE_IDX]);

	if (status != 0) {
		bds_cache_destroy(serdes_caches[SERDES_PAYLOAD_CACHE_IDX], 0);
		sfs_log(serdes_ctx, SFS_CRIT, "%s(): %d data4k-cache failed\n",
				__FUNCTION__, __LINE__);
		return -ENOMEM;
	}
	sfs_log(serdes_ctx, SFS_DEBUG, "%s(): %d %p- data4k-cache created\n",
			__FUNCTION__, __LINE__, serdes_caches[SERDES_DATA_4K_CACHE_IDX]);

	/* Create data cache of size 64K */
	status = bds_cache_create("data64k-cache", 65536,
							  0, NULL, NULL,
							  &serdes_caches[SERDES_DATA_64K_CACHE_IDX]);

	if (status != 0) {
		bds_cache_destroy(serdes_caches[SERDES_DATA_4K_CACHE_IDX], 0);
		bds_cache_destroy(serdes_caches[SERDES_PAYLOAD_CACHE_IDX], 0);
		sfs_log(serdes_ctx, SFS_CRIT, "%s(): %d data64k-cache failed\n",
				__FUNCTION__, __LINE__);
		return -ENOMEM;
	}
	sfs_log(serdes_ctx, SFS_DEBUG, "%s(): %d %p- data64k-cache created\n",
			__FUNCTION__, __LINE__, serdes_caches[SERDES_DATA_64K_CACHE_IDX]);

	/* All OK. */
	*cache_array = serdes_caches;
	return 0;
		
}

/*
 * sstack_command_stringify - blurt out the command in string
 *
 * This is needed to make the send and recv code better.
 * Otherwise, a totally dummy function.
 */

char *
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
		case NFS_ESURE_CODE: return "NFS_ESURE_CODE";
		case NFS_ESURE_CODE_RSP: return "NFS_ESURE_CODE_RSP";
		case NFS_MKDIR_RSP: return "NFS_MKDIR_RSP";
		case NFS_SYMLINK_RSP: return "NFS_SYMLINK_RSP";
		case NFS_MKNOD_RSP: return "NFS_MKNOD_RSP";
		case NFS_REMOVE_RSP: return "NFS_REMOVE_RSP";
		case NFS_RMDIR_RSP: return "NFS_RMDIR_RSP";
		case NFS_RENAME_RSP: return "NFS_RENAME_RSP";
		case NFS_LINK_RSP: return "NFS_LINK_RSP";
		case NFS_READDIR_RSP: return "NFS_READDIR_RSP";
		case NFS_READDIRPLUS_RSP: return "NFS_READDIRPLUS_RSP";
		case NFS_FSSTAT_RSP: return "NFS_FSSTAT_RSP";
		case NFS_FSINFO_RSP: return "NFS_FSINFO_RSP";
		case NFS_PATHCONF_RSP: return "NFS_PATHCONF_RSP";
		case NFS_COMMIT_RSP: return "NFS_COMMIT_RSP";
		case NFS_SETATTR_RSP: return "NFS_SETATTR_RSP";
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
	sfs_log(transport->ctx, SFS_DEBUG, "%s: %d %d\n", __FUNCTION__, __LINE__,
				(int) handle);
	if (transport && transport->transport_ops.tx &&
			transport->transport_ops.rx  && (handle > 0)) {
		int ret = -1;
		sfs_log(transport->ctx, SFS_DEBUG, "%s: %d %d\n", __FUNCTION__,
				__LINE__, (int) handle);
		if (tx == 1 && transport->transport_ops.tx)
			ret = transport->transport_ops.tx(handle, len, (void *) buffer,
					transport->ctx);
		else if (tx == 0 && transport->transport_ops.rx)
			ret = transport->transport_ops.rx(handle, len, (void *) buffer,
					transport->ctx);
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
 * job_id - Job id as maintained by sfs. sfsd does not alter this
 * job - back pointer to job structure. sfsd does not alter this
 * priority - priority of the job. sfsd does not alter this
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
					sstack_job_id_t job_id,
					sfs_job_t *job,
					int priority, log_ctx_t *ctx)
{

	SstackPayloadT msg = SSTACK_PAYLOAD_T__INIT;
	SstackPayloadHdrT hdr = SSTACK_PAYLOAD_HDR_T__INIT;
	SstackNfsCommandStruct cmd = SSTACK_NFS_COMMAND_STRUCT__INIT;
	SstackNfsResponseStruct response = SSTACK_NFS_RESPONSE_STRUCT__INIT;
	uint32_t payload_len = 0;
	int ret = -1;
	size_t len = 0;
	char *buffer = NULL;

	sfs_log(ctx, SFS_DEBUG, "%s: handle = %d payload 0x%x transport 0x%x "
			"job_id = %d \n", __FUNCTION__, handle, payload, transport, job_id);

	// Parameter validation
	if (handle == -1 || NULL == payload || NULL == transport ||
					job_id < 0 || job_id >= MAX_OUTSTANDING_JOBS) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid paramaters specified \n",
						__FUNCTION__);
		return -EINVAL;
	}

	hdr.sequence = sequence++;
	hdr.payload_len = 0; // Dummy . will be filled later
	hdr.job_id = job_id;
	hdr.priority = priority;
	hdr.arg = (uint64_t) job;
	msg.hdr = &hdr;
	sfs_log(ctx, SFS_DEBUG, "%s %d \n", __FUNCTION__, __LINE__);

	switch (payload->command) {
		case SSTACK_ADD_STORAGE:
		case SSTACK_REMOVE_STORAGE:
		case SSTACK_UPDATE_STORAGE: {
			SfsdStorageT storage = SFSD_STORAGE_T__INIT;
			SstackAddressT addr = SSTACK_ADDRESS_T__INIT;

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
					payload->storage.address.ipv4_address :
					payload->storage.address.ipv6_address);
			if (payload->command == SSTACK_ADD_STORAGE)
				msg.command =
					SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__SSTACK_ADD_STORAGE;
			else if (payload->command == SSTACK_REMOVE_STORAGE)
				msg.command =
				 SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__SSTACK_REMOVE_STORAGE;
			else
				msg.command =
				 SSTACK_PAYLOAD_T__SSTACK_NFS_COMMAND_T__SSTACK_UPDATE_STORAGE;
			sfs_log(ctx, SFS_DEBUG, "%s: %d\n", __FUNCTION__, __LINE__);
			storage.path.len = strlen(payload->storage.path);
			sfs_log(ctx, SFS_DEBUG, "%s: %d\n", __FUNCTION__, __LINE__);
			storage.path.data = malloc(storage.path.len + 1);
			strncpy((char *) storage.path.data, payload->storage.path,
					storage.path.len);
			storage.mount_point.len = strlen(payload->storage.mount_point);
			storage.mount_point.data = malloc(storage.mount_point.len);
			strncpy((char *)storage.mount_point.data,
							payload->storage.mount_point,
							storage.mount_point.len);
			storage.weight = payload->storage.weight;
			storage.nblocks = payload->storage.nblocks;
			storage.protocol = payload->storage.protocol;
			storage.num_chunks_written = payload->storage.num_chunks_written;
			if (transport->transport_hdr.tcp.ipv4) {
				sfs_log(ctx, SFS_DEBUG, "%s: %d\n", __FUNCTION__, __LINE__);
				addr.protocol = IPV4;
				addr.has_ipv4_address = true;
				addr.ipv4_address.len = IPV4_ADDR_MAX;
				addr.ipv4_address.data = malloc(addr.ipv4_address.len);
				strcpy((char *) addr.ipv4_address.data,
					payload->storage.address.ipv4_address);
				sfs_log(ctx, SFS_DEBUG, "%s: %d\n", __FUNCTION__, __LINE__);
				storage.address = &addr;
			} else {
				addr.protocol = IPV6;
				addr.has_ipv6_address = true;
				addr.ipv6_address.len = IPV6_ADDR_MAX;
				addr.ipv6_address.data = malloc(addr.ipv6_address.len);
				strcpy((char *) addr.ipv6_address.data,
					payload->storage.address.ipv6_address);
				storage.address = &addr;
			}
			msg.storage = &storage;
			sfs_log(ctx, SFS_DEBUG, "%s %d %d\n", __FUNCTION__, __LINE__, len);
			hdr.payload_len = sizeof(sstack_payload_hdr_t);
			sfs_log(ctx, SFS_DEBUG, "%s %d %d %d\n", __FUNCTION__, __LINE__,
						hdr.payload_len, sizeof(sstack_payload_hdr_t));
			msg.hdr = &hdr; // Parannoid
			len = sstack_payload_t__get_packed_size(&msg);
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
						payload->storage.address.ipv4_address :
						payload->storage.address.ipv6_address,
					payload->storage.path, ret);

				free(buffer);

				return -ret;
			} else {
				sfs_log(ctx, SFS_INFO, "%s: Successfully xmitted payload "
					"for %s request for ipaddr %s "
					"path %s \n", __FUNCTION__,
					sstack_command_stringify(payload->command),
					(transport->transport_hdr.tcp.ipv4) ?
						payload->storage.address.ipv4_address :
						payload->storage.address.ipv6_address,
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
			hdr.payload_len = sizeof(sstack_payload_hdr_t);
			msg.hdr = &hdr; // Parannoid
			len = sstack_payload_t__get_packed_size(&msg);
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
			hdr.payload_len = sizeof(sstack_payload_hdr_t);
			msg.hdr = &hdr; // Parannoid
			len = sstack_payload_t__get_packed_size(&msg);
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
			hdr.payload_len = sizeof(sstack_payload_hdr_t);
			msg.hdr = &hdr; // Parannoid
			len = sstack_payload_t__get_packed_size(&msg);
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
			readcmd.inode_no = payload->command_struct.read_cmd.inode_no;
			readcmd.offset = payload->command_struct.read_cmd.offset;
			readcmd.count = payload->command_struct.read_cmd.count;
			readcmd.read_ecode = payload->command_struct.read_cmd.read_ecode;
			attr.ver = payload->command_struct.read_cmd.pe.pe_attr.ver;
			attr.a_qoslevel =
				payload->command_struct.read_cmd.pe.pe_attr.a_qoslevel;
			attr.a_ishidden =
				payload->command_struct.read_cmd.pe.pe_attr.a_ishidden;
			attr.a_numreplicas =
				payload->command_struct.read_cmd.pe.pe_attr.a_numreplicas;
			attr.a_enable_dr =
				payload->command_struct.read_cmd.pe.pe_attr.a_enable_dr;
			entry.pe_attr = &attr;
			entry.pe_num_plugins =
				payload->command_struct.read_cmd.pe.pe_num_plugins;
			entry.pe_refcount =
				payload->command_struct.read_cmd.pe.pe_refcount;
			entry.pe_lock =
				payload->command_struct.read_cmd.pe.pe_lock;
			entry.pst_index =
				payload->command_struct.read_cmd.pe.pst_index;
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
					payload->command_struct.read_cmd.pe.pe_policy[i]->ver;
				plugins[i]->pp_refcount =
					payload->command_struct.read_cmd.pe.pe_policy[i]
						->pp_refcount;
				plugins[i]->pp_policy_name.len =
					strlen(payload->command_struct.read_cmd.pe.pe_policy[i]
						->pp_policy_name);
				strcpy((char *)&plugins[i]->pp_policy_name.data,
					payload->command_struct.read_cmd.pe.pe_policy[i]
						->pp_policy_name);
				plugins[i]->pp_sha_sum.len =
					strlen(payload->command_struct.read_cmd.pe.pe_policy[i]
						->pp_sha_sum);
				strcpy((char *) &plugins[i]->pp_sha_sum.data,
					payload->command_struct.read_cmd.pe.pe_policy[i]
						->pp_sha_sum);
				plugins[i]->is_activated =
					payload->command_struct.read_cmd.pe.pe_policy[i]
						->is_activated;
				plugins[i]->pp_lock =
					payload->command_struct.read_cmd.pe.pe_policy[i]
						->pp_lock;
			}
			entry.pe_policy = plugins;
			readcmd.pe = &entry;
			cmd.read_cmd = &readcmd;
			msg.command_struct = &cmd;
			hdr.payload_len = sizeof(sstack_payload_hdr_t);
			msg.hdr = &hdr; // Parannoid
			len = sstack_payload_t__get_packed_size(&msg);
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
			data.data_buf.len = data.data_len;
			strncpy((char *) &data.data_buf.data,
				(char *) &payload->command_struct.write_cmd.data.data_buf,
				data.data_len);
			writecmd.inode_no = payload->command_struct.write_cmd.inode_no;
			writecmd.offset = payload->command_struct.write_cmd.offset;
			writecmd.count = payload->command_struct.write_cmd.count;
			writecmd.data = &data;
			attr.ver = payload->command_struct.write_cmd.pe.pe_attr.ver;
			attr.a_qoslevel =
				payload->command_struct.write_cmd.pe.pe_attr.a_qoslevel;
			attr.a_ishidden =
				payload->command_struct.write_cmd.pe.pe_attr.a_ishidden;
			attr.a_numreplicas =
				payload->command_struct.write_cmd.pe.pe_attr.a_numreplicas;
			attr.a_enable_dr =
				payload->command_struct.write_cmd.pe.pe_attr.a_enable_dr;
			entry.pe_attr = &attr;
			entry.pe_num_plugins =
				payload->command_struct.write_cmd.pe.pe_num_plugins;
			entry.pe_refcount =
				payload->command_struct.write_cmd.pe.pe_refcount;
			entry.pe_lock =
				payload->command_struct.write_cmd.pe.pe_lock;
			entry.pst_index =
				payload->command_struct.write_cmd.pe.pst_index;
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
					payload->command_struct.write_cmd.pe.pe_policy[i]->ver;
				plugins[i]->pp_refcount =
					payload->command_struct.write_cmd.pe.pe_policy[i]
						->pp_refcount;
				plugins[i]->pp_policy_name.len =
					strlen(payload->command_struct.write_cmd.pe.pe_policy[i]
						->pp_policy_name);
				strcpy((char *)&plugins[i]->pp_policy_name.data,
					payload->command_struct.write_cmd.pe.pe_policy[i]
						->pp_policy_name);
				plugins[i]->pp_sha_sum.len =
					strlen(payload->command_struct.write_cmd.pe.pe_policy[i]
						->pp_sha_sum);
				strcpy((char *) &plugins[i]->pp_sha_sum.data,
					payload->command_struct.write_cmd.pe.pe_policy[i]
						->pp_sha_sum);
				plugins[i]->is_activated =
					payload->command_struct.write_cmd.pe.pe_policy[i]
						->is_activated;
				plugins[i]->pp_lock =
					payload->command_struct.write_cmd.pe.pe_policy[i]
						->pp_lock;
			}
			entry.pe_policy = plugins;
			writecmd.pe = &entry;
			cmd.write_cmd = &writecmd;
			msg.command_struct = &cmd;
			hdr.payload_len = sizeof(sstack_payload_hdr_t);
			msg.hdr = &hdr; // Parannoid
			len = sstack_payload_t__get_packed_size(&msg);
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
			data.data_buf.len = data.data_len;
			strncpy((char *) &data.data_buf.data,
				(char *) &payload->command_struct.create_cmd.data.data_buf,
				data.data_len);
			createcmd.inode_no = payload->command_struct.create_cmd.inode_no;
			createcmd.mode = payload->command_struct.create_cmd.mode;
			createcmd.data = &data;
			attr.ver = payload->command_struct.create_cmd.pe.pe_attr.ver;
			attr.a_qoslevel =
				payload->command_struct.create_cmd.pe.pe_attr.a_qoslevel;
			attr.a_ishidden =
				payload->command_struct.create_cmd.pe.pe_attr.a_ishidden;
			attr.a_numreplicas =
				payload->command_struct.create_cmd.pe.pe_attr.a_numreplicas;
			attr.a_enable_dr =
				payload->command_struct.create_cmd.pe.pe_attr.a_enable_dr;
			entry.pe_attr = &attr;
			entry.pe_num_plugins =
				payload->command_struct.create_cmd.pe.pe_num_plugins;
			entry.pe_refcount =
				payload->command_struct.create_cmd.pe.pe_refcount;
			entry.pe_lock =
				payload->command_struct.create_cmd.pe.pe_lock;
			entry.pst_index =
				payload->command_struct.create_cmd.pe.pst_index;
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
					payload->command_struct.create_cmd.pe.pe_policy[i]->ver;
				plugins[i]->pp_refcount =
					payload->command_struct.create_cmd.pe.pe_policy[i]
						->pp_refcount;
				plugins[i]->pp_policy_name.len =
					strlen(payload->command_struct.create_cmd.pe.pe_policy[i]
						->pp_policy_name);
				strcpy((char *)&plugins[i]->pp_policy_name.data,
					payload->command_struct.create_cmd.pe.pe_policy[i]
						->pp_policy_name);
				plugins[i]->pp_sha_sum.len =
					strlen(payload->command_struct.create_cmd.pe.pe_policy[i]
						->pp_sha_sum);
				strcpy((char *) &plugins[i]->pp_sha_sum.data,
					payload->command_struct.create_cmd.pe.pe_policy[i]
						->pp_sha_sum);
				plugins[i]->is_activated =
					payload->command_struct.create_cmd.pe.pe_policy[i]
						->is_activated;
				plugins[i]->pp_lock =
					payload->command_struct.create_cmd.pe.pe_policy[i]
						->pp_lock;
			}
			entry.pe_policy = plugins;
			createcmd.pe = &entry;
			cmd.create_cmd = &createcmd;
			msg.command_struct = &cmd;
			hdr.payload_len = sizeof(sstack_payload_hdr_t);
			msg.hdr = &hdr; // Parannoid
			len = sstack_payload_t__get_packed_size(&msg);
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
			hdr.payload_len = sizeof(sstack_payload_hdr_t);
			msg.hdr = &hdr; // Parannoid
			len = sstack_payload_t__get_packed_size(&msg);
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
			hdr.payload_len = sizeof(sstack_payload_hdr_t);
			msg.hdr = &hdr; // Parannoid
			len = sstack_payload_t__get_packed_size(&msg);
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
			hdr.payload_len = sizeof(sstack_payload_hdr_t);
			msg.hdr = &hdr; // Parannoid
			len = sstack_payload_t__get_packed_size(&msg);
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
			hdr.payload_len = sizeof(sstack_payload_hdr_t);
			msg.hdr = &hdr; // Parannoid
			len = sstack_payload_t__get_packed_size(&msg);
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
			hdr.payload_len = sizeof(sstack_payload_hdr_t);
			msg.hdr = &hdr; // Parannoid
			len = sstack_payload_t__get_packed_size(&msg);
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
		// TODO
		// All these commands only return command_ok
		// Check if any of them require modifications
		case SSTACK_REMOVE_STORAGE_RSP:
		case SSTACK_UPDATE_STORAGE_RSP:
		case SSTACK_ADD_STORAGE_RSP:
		case NFS_ESURE_CODE_RSP:
		case NFS_MKDIR_RSP:
		case NFS_SYMLINK_RSP:
		case NFS_MKNOD_RSP:
		case NFS_REMOVE_RSP:
		case NFS_RMDIR_RSP:
		case NFS_RENAME_RSP:
		case NFS_LINK_RSP:
		case NFS_READDIR_RSP:
		case NFS_READDIRPLUS_RSP:
		case NFS_FSSTAT_RSP:
		case NFS_FSINFO_RSP:
		case NFS_PATHCONF_RSP:
		case NFS_COMMIT_RSP:
		case NFS_SETATTR_RSP: {
			response.command_ok = payload->response_struct.command_ok;
			response.handle = payload->response_struct.handle;
			msg.command = payload->command;
			msg.response_struct = &response;
			// hdr.payload_len = len - sizeof(sstack_payload_hdr_t);
			hdr.payload_len = sizeof(sstack_payload_t);
			sfs_log(ctx, SFS_DEBUG, "%s: Payload len sent is %d\n",
					__FUNCTION__,  len);
			msg.hdr = &hdr; // Parannoid
			len = sstack_payload_t__get_packed_size(&msg);
			sfs_log(ctx, SFS_DEBUG, "%s: Command = %s len = %d\n",
					__FUNCTION__, sstack_command_stringify(payload->command),
					len);
			buffer = malloc(len);
			if (NULL == buffer) {
				sfs_log(ctx, SFS_ERR, "%s: Unable to allocate memory for "
					"%s. Command aborted \n", __FUNCTION__,
					sstack_command_stringify(payload->command));

				return -ENOMEM;
			}
			sfs_log(ctx, SFS_DEBUG, "%s: Allocated length is %d\n",
					__FUNCTION__, len);

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
		case NFS_GETATTR_RSP: {
			SstackNfsGetattrResp getattr_resp = SSTACK_NFS_GETATTR_RESP__INIT;
			Stat stat = STAT__INIT;

			memcpy((void *) &stat.st_dev,
					(void *)&payload->response_struct.getattr_resp.stbuf,
					sizeof(struct stat));
			getattr_resp.stbuf = &stat;
			response.getattr_resp = &getattr_resp;
			msg.response_struct = &response;
			hdr.payload_len = sizeof(sstack_payload_hdr_t);
			msg.hdr = &hdr; // Parannoid
			len = sstack_payload_t__get_packed_size(&msg);
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
			hdr.payload_len = sizeof(sstack_payload_hdr_t);
			msg.hdr = &hdr; // Parannoid
			len = sstack_payload_t__get_packed_size(&msg);
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
			hdr.payload_len = sizeof(sstack_payload_hdr_t);
			msg.hdr = &hdr; // Parannoid
			len = sstack_payload_t__get_packed_size(&msg);
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
			data.data_buf.len = data.data_len;
			memcpy((void *) &data.data_buf.data,
					(void*) payload->response_struct.read_resp.data.data_buf,
					data.data_len);
			read_resp.data = &data;
			response.read_resp = &read_resp;
			msg.response_struct = &response;
			hdr.payload_len = sizeof(sstack_payload_hdr_t);
			msg.hdr = &hdr; // Parannoid
			len = sstack_payload_t__get_packed_size(&msg);
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
			hdr.payload_len = sizeof(sstack_payload_hdr_t);
			msg.hdr = &hdr; // Parannoid
			len = sstack_payload_t__get_packed_size(&msg);
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
			hdr.payload_len = sizeof(sstack_payload_hdr_t);
			msg.hdr = &hdr; // Parannoid
			len = sstack_payload_t__get_packed_size(&msg);
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
			hdr.payload_len = sizeof(sstack_payload_hdr_t);
			msg.hdr = &hdr; // Parannoid
			len = sstack_payload_t__get_packed_size(&msg);
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

	sfs_log(ctx, SFS_DEBUG, "%s %d \n", __FUNCTION__, __LINE__);

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

	// Parameter validation
	if (handle == -1 || NULL == transport) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid paramaters specified \n",
						__FUNCTION__);

		errno = EINVAL;
		return NULL;
	}
	sfs_log(ctx, SFS_DEBUG, "%s %d \n", __FUNCTION__, __LINE__);

	payload = (sstack_payload_t *) malloc(sizeof(sstack_payload_t));
	if (NULL == payload) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to allocate memory for receiving"
			" payload. Client handle = %"PRId64" \n",
			__FUNCTION__, handle);

		return NULL;
	}

	memset((void *)payload, '\0', sizeof(sstack_payload_t));

	sfs_log(ctx, SFS_DEBUG, "%s: handle = %d ipv4_addr = %s \n",
			__FUNCTION__, handle, transport->transport_hdr.tcp.ipv4_addr);

	// Call the transport function to receive the buffer
	payload_len = _sendrecv_payload(transport, handle, temp_payload,
			 (size_t) sizeof(sstack_payload_t), 0);
	if (payload_len == -1) {
		sfs_log(ctx, SFS_ERR, "%s: Recv failed for payload. "
			"Client handle = %"PRId64" \n",
			__FUNCTION__, handle);

		return NULL;
	}

	sfs_log(ctx, SFS_DEBUG, "%s: Payload len received is %d\n",
			__FUNCTION__, payload_len);
	// Parse temp_payload and populate payload
	msg = sstack_payload_t__unpack(NULL, payload_len, temp_payload);
	// Populate the payload structure with required fields
	payload->hdr.sequence = msg->hdr->sequence;
	payload->hdr.payload_len = msg->hdr->payload_len;
	payload->hdr.job_id = msg->hdr->job_id;
	payload->hdr.priority = msg->hdr->priority;
	payload->hdr.arg = msg->hdr->arg;
	payload->command = msg->command;

	sfs_log(ctx, SFS_DEBUG, "%s: payload_len %d job_id %d priority %d "
			"command %d \n", __FUNCTION__, payload->hdr.payload_len,
			payload->hdr.job_id, payload->hdr.priority, payload->command);

	switch(msg->command) {
		case SSTACK_ADD_STORAGE:
		case SSTACK_REMOVE_STORAGE:
		case SSTACK_UPDATE_STORAGE: {
			ProtobufCBinaryData data = msg->storage->path;
			ProtobufCBinaryData mnt_pnt = msg->storage->mount_point;

			strncpy(payload->storage.path, (char *) data.data, data.len);
			sfs_log(ctx, SFS_DEBUG, "%s: storage.path = %s\n",
					__FUNCTION__, payload->storage.path);
			strncpy(payload->storage.mount_point, (char *) mnt_pnt.data,
					mnt_pnt.len);
			payload->storage.weight = msg->storage->weight;
			payload->storage.nblocks = msg->storage->nblocks;
			payload->storage.protocol = msg->storage->protocol;
			if (transport->transport_hdr.tcp.ipv4) {
				ProtobufCBinaryData addr =
					msg->storage->address->ipv4_address;
				strcpy(payload->storage.address.ipv4_address,
								(char *) addr.data);
				sfs_log(ctx, SFS_DEBUG, "%s: ipv4_addr = %s\n",
						__FUNCTION__, payload->storage.address.ipv4_address);
			} else {
				ProtobufCBinaryData addr =
					msg->storage->address->ipv6_address;
				strcpy(payload->storage.address.ipv6_address,
								(char *) addr.data);
				sfs_log(ctx, SFS_DEBUG, "%s: ipv6_addr = %s\n",
						__FUNCTION__, payload->storage.address.ipv6_address);
			}

			return payload;
		}
		// TODO
		// Check some of the responses for correctness
		// All of these only return command_ok
		case SSTACK_ADD_STORAGE_RSP:
		case SSTACK_REMOVE_STORAGE_RSP:
		case SSTACK_UPDATE_STORAGE_RSP:
		case NFS_ESURE_CODE_RSP:
		case NFS_MKDIR_RSP:
		case NFS_SYMLINK_RSP:
		case NFS_MKNOD_RSP:
		case NFS_REMOVE_RSP:
		case NFS_RMDIR_RSP:
		case NFS_RENAME_RSP:
		case NFS_LINK_RSP:
		case NFS_READDIR_RSP:
		case NFS_READDIRPLUS_RSP:
		case NFS_FSSTAT_RSP:
		case NFS_FSINFO_RSP:
		case NFS_PATHCONF_RSP:
		case NFS_COMMIT_RSP:
		case NFS_SETATTR_RSP: {
			sfs_log(ctx, SFS_INFO, "%s: command = %s\n",
					__FUNCTION__, sstack_command_stringify(msg->command));
			payload->response_struct.command_ok =
				msg->response_struct->command_ok;
			payload->response_struct.handle =
				msg->response_struct->handle;
			sfs_log(ctx, SFS_DEBUG, "response status: %d\n",
					payload->response_struct.command_ok);
			if (payload->response_struct.command_ok == SSTACK_SUCCESS) {
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
							data.data);
			data = msg->command_struct->lookup_cmd->what->name;
			strcpy(payload->command_struct.lookup_cmd.what.name, (char *)
							data.data);
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

			payload->command_struct.read_cmd.inode_no =
					msg->command_struct->read_cmd->inode_no;
			payload->command_struct.read_cmd.offset =
					msg->command_struct->read_cmd->offset;
			payload->command_struct.read_cmd.count =
					msg->command_struct->read_cmd->count;
			payload->command_struct.read_cmd.read_ecode =
					msg->command_struct->read_cmd->read_ecode;
			entry = msg->command_struct->read_cmd->pe;
			attr = entry->pe_attr;
			memcpy( (void *) &payload->command_struct.read_cmd.pe.pe_attr,
							(void *) &attr->ver, sizeof(struct attribute));
			payload->command_struct.read_cmd.pe.pe_num_plugins =
					entry->pe_num_plugins;
			payload->command_struct.read_cmd.pe.pe_refcount =
					entry->pe_refcount;
			payload->command_struct.read_cmd.pe.pe_lock =
					entry->pe_lock;
			payload->command_struct.read_cmd.pe.pst_index =
					entry->pst_index;
			plugins = msg->command_struct->read_cmd->pe->pe_policy;
			for ( i = 0; i < entry->pe_num_plugins; i++ ) {
				payload->command_struct.read_cmd.pe.pe_policy[i]->ver =
						plugins[i]->ver;
				payload->command_struct.read_cmd.pe.
					pe_policy[i]->pp_refcount = plugins[i]->pp_refcount;
				strcpy(payload->command_struct.read_cmd.pe.
					pe_policy[i]->pp_policy_name,
					(char*) &plugins[i]->pp_policy_name.data);
				strcpy(payload->command_struct.read_cmd.pe.
					pe_policy[i]->pp_sha_sum,
					(char *) &plugins[i]->pp_sha_sum.data);

				payload->command_struct.read_cmd.pe.
					pe_policy[i]->is_activated = plugins[i]->is_activated;
				payload->command_struct.read_cmd.pe.pe_policy[i]->pp_lock =
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

			payload->command_struct.write_cmd.inode_no =
					msg->command_struct->write_cmd->inode_no;
			payload->command_struct.write_cmd.offset =
					msg->command_struct->write_cmd->offset;
			payload->command_struct.write_cmd.count =
					msg->command_struct->write_cmd->count;
			data = msg->command_struct->write_cmd->data;
			payload->command_struct.write_cmd.data.data_len = data->data_len;
			data1 = msg->command_struct->write_cmd->data->data_buf;
			strcpy((char *) &payload->command_struct.write_cmd.data.data_buf,
							(char *) data1.data);

			entry = msg->command_struct->write_cmd->pe;
			attr = entry->pe_attr;
			memcpy( (void *) &payload->command_struct.write_cmd.pe.pe_attr,
							(void *) &attr->ver, sizeof(struct attribute));
			payload->command_struct.write_cmd.pe.pe_num_plugins =
					entry->pe_num_plugins;
			payload->command_struct.write_cmd.pe.pe_refcount =
					entry->pe_refcount;
			payload->command_struct.write_cmd.pe.pe_lock =
					entry->pe_lock;
			payload->command_struct.write_cmd.pe.pst_index =
					entry->pst_index;
			plugins = msg->command_struct->write_cmd->pe->pe_policy;
			for ( i = 0; i < entry->pe_num_plugins; i++ ) {
				payload->command_struct.write_cmd.pe.pe_policy[i]->ver =
						plugins[i]->ver;
				payload->command_struct.write_cmd.pe.
					pe_policy[i]->pp_refcount = plugins[i]->pp_refcount;
				strcpy(payload->command_struct.write_cmd.pe.
					pe_policy[i]->pp_policy_name,
					(char*) &plugins[i]->pp_policy_name.data);
				strcpy(payload->command_struct.write_cmd.pe.
					pe_policy[i]->pp_sha_sum,
					(char *) &plugins[i]->pp_sha_sum.data);

				payload->command_struct.write_cmd.pe.
					pe_policy[i]->is_activated = plugins[i]->is_activated;
				payload->command_struct.write_cmd.pe.pe_policy[i]->pp_lock =
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

			payload->command_struct.create_cmd.inode_no =
					msg->command_struct->create_cmd->inode_no;
			payload->command_struct.create_cmd.mode =
					msg->command_struct->create_cmd->mode;
			data = msg->command_struct->create_cmd->data;
			payload->command_struct.create_cmd.data.data_len = data->data_len;
			data1 = msg->command_struct->create_cmd->data->data_buf;
			strcpy((char *) &payload->command_struct.create_cmd.data.data_buf,
							(char *) data1.data);

			entry = msg->command_struct->create_cmd->pe;
			attr = entry->pe_attr;
			memcpy( (void *) &payload->command_struct.create_cmd.pe.pe_attr,
							(void *) &attr->ver, sizeof(struct attribute));
			payload->command_struct.create_cmd.pe.pe_num_plugins =
					entry->pe_num_plugins;
			payload->command_struct.create_cmd.pe.pe_refcount =
					entry->pe_refcount;
			payload->command_struct.create_cmd.pe.pe_lock =
					entry->pe_lock;
			payload->command_struct.create_cmd.pe.pst_index =
					entry->pst_index;
			plugins = msg->command_struct->create_cmd->pe->pe_policy;
			for ( i = 0; i < entry->pe_num_plugins; i++ ) {
				payload->command_struct.create_cmd.pe.pe_policy[i]->ver =
						plugins[i]->ver;
				payload->command_struct.create_cmd.pe.
					pe_policy[i]->pp_refcount = plugins[i]->pp_refcount;
				strcpy(payload->command_struct.create_cmd.pe.
					pe_policy[i]->pp_policy_name,
					(char*) &plugins[i]->pp_policy_name.data);
				strcpy(payload->command_struct.create_cmd.pe.
					pe_policy[i]->pp_sha_sum,
					(char *) &plugins[i]->pp_sha_sum.data);

				payload->command_struct.create_cmd.pe.
					pe_policy[i]->is_activated = plugins[i]->is_activated;
				payload->command_struct.create_cmd.pe.pe_policy[i]->pp_lock =
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
							data.data);
			return payload;
		}
		case NFS_RENAME: {
			SstackFileNameT *filename = NULL;
			ProtobufCBinaryData data;

			payload->command_struct.rename_cmd.new_path.name_len =
					msg->command_struct->rename_cmd->new_path->name_len;
			data = msg->command_struct->rename_cmd->new_path->name;
			strcpy(payload->command_struct.rename_cmd.new_path.name, (char *)
							data.data);
			return payload;
		}

		case NFS_REMOVE: {
			ProtobufCBinaryData path;

			payload->command_struct.remove_cmd.path_len =
					msg->command_struct->remove_cmd->path_len;
			path = msg->command_struct->remove_cmd->path;
			strcpy((char *) &payload->command_struct.remove_cmd.path,
							(char *) path.data);

			return payload;
		}

		case NFS_COMMIT:
			payload->command_struct.commit_cmd.offset =
					msg->command_struct->commit_cmd->offset;
			payload->command_struct.commit_cmd.count =
					msg->command_struct->commit_cmd->count;

			return payload;

		case NFS_GETATTR_RSP:
			payload->response_struct.command_ok =
					msg->response_struct->command_ok;
			payload->response_struct.handle =
				msg->response_struct->handle;
			memcpy((void *) &payload->response_struct.getattr_resp.stbuf,
					(void *) msg->response_struct->getattr_resp->stbuf,
					sizeof(struct stat));

			return payload;
		case NFS_LOOKUP_RSP: {
			ProtobufCBinaryData lookup_path;

			payload->response_struct.command_ok =
					msg->response_struct->command_ok;
			payload->response_struct.handle =
				msg->response_struct->handle;
			payload->response_struct.lookup_resp.lookup_path_len =
				msg->response_struct->lookup_resp->lookup_path_len;
			lookup_path = msg->response_struct->lookup_resp->lookup_path;
			memcpy((void *) payload->response_struct.lookup_resp.lookup_path,
				(void *) lookup_path.data, lookup_path.len);
			return payload;
		}
		case NFS_ACCESS_RSP:
			payload->response_struct.command_ok =
					msg->response_struct->command_ok;
			payload->response_struct.handle =
				msg->response_struct->handle;
			payload->response_struct.access_resp.access =
				msg->response_struct->access_resp->access;
			return payload;
		case NFS_READLINK_RSP: {
			ProtobufCBinaryData name;

			payload->response_struct.command_ok =
					msg->response_struct->command_ok;
			payload->response_struct.handle =
				msg->response_struct->handle;
			payload->response_struct.readlink_resp.real_file.name_len =
				msg->response_struct->readlink_resp->real_file->name_len;
			name = msg->response_struct->readlink_resp->real_file->name;
			memcpy((void *) payload->response_struct.readlink_resp.
				real_file.name, (void *) name.data, name.len);
			return payload;
		}
		case NFS_READ_RSP: {
			ProtobufCBinaryData data_buf;

			payload->response_struct.command_ok =
					msg->response_struct->command_ok;
			payload->response_struct.handle =
				msg->response_struct->handle;
			payload->response_struct.read_resp.count =
				msg->response_struct->read_resp->count;
			payload->response_struct.read_resp.eof =
				msg->response_struct->read_resp->eof;
			payload->response_struct.read_resp.data.data_len =
				msg->response_struct->read_resp->data->data_len;
			data_buf = msg->response_struct->read_resp->data->data_buf;
			memcpy((void *) payload->response_struct.read_resp.data.data_buf,
				(void *) data_buf.data, data_buf.len);
			return payload;
		}
		case NFS_WRITE_RSP:
			payload->response_struct.command_ok =
					msg->response_struct->command_ok;
			payload->response_struct.handle =
				msg->response_struct->handle;
			payload->response_struct.write_resp.file_create_ok =
				msg->response_struct->write_resp->file_create_ok;
			payload->response_struct.write_resp.file_wc =
				msg->response_struct->write_resp->file_wc;
			return payload;
		case NFS_CREATE_RSP:
			payload->response_struct.command_ok =
					msg->response_struct->command_ok;
			payload->response_struct.handle =
				msg->response_struct->handle;
			payload->response_struct.create_resp.file_create_ok =
				msg->response_struct->create_resp->file_create_ok;
			payload->response_struct.create_resp.file_wc =
				msg->response_struct->create_resp->file_wc;
			return payload;

	}
	return NULL;
}
