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
#include <pthread.h>
#include <stdint.h>
#include <sstack_sfsd.h>
#include <sstack_log.h>
#include <sstack_chunk.h>
#include <sstack_jobs.h>
#include <sfsd_ops.h>
#include <sstack_md.h>
#include <sstack_types.h>
#include <sstack_serdes.h>

extern bds_cache_desc_t sfsd_global_cache_arr[];
/* ===================== PRIVATE DECLARATIONS ============================ */

static void* do_receive_thread (void *params);
static void handle_command(sstack_payload_t *command,
		sstack_payload_t **response,
		bds_cache_desc_t cache_arr[2], sfsd_t *sfsd,
		log_ctx_t *log_ctx);
static inline void send_unsucessful_response(sstack_payload_t *payload,
		sfsd_t *sfsd);
static sstack_payload_t* get_payload(sstack_transport_t *transport,
		sstack_client_handle_t handle);
static int32_t send_payload(sstack_transport_t *transport,
		sstack_client_handle_t handle, sstack_payload_t *response);

/* ====================+++++++++++++++++++++++++++++++++==================*/
static inline void send_unsucessful_response(sstack_payload_t *payload,
		sfsd_t *sfsd)
{
	if (payload) {
		payload->response_struct.command_ok = -1;
		send_payload(sfsd->transport, sfsd->handle, payload);
	}
}

static int32_t sfsd_create_receiver_thread(sfsd_t *sfsd)
{
	int32_t ret = 0;
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

	ret = pthread_create(&sfsd->receiver_thread, &attr,
			do_receive_thread, sfsd);
	SFS_LOG_EXIT((ret == 0), "Receiver thread create failed\n", ret,
			sfsd->log_ctx, SFS_ERR);

	sfs_log(sfsd->log_ctx, SFS_INFO, "Receive thread creation successful\n");
	return 0;
}

int32_t init_thread_pool(sfsd_t *sfsd)
{
	int32_t ret = 0;
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

	ret = sfsd_create_receiver_thread(sfsd);
	SFS_LOG_EXIT((ret == 0), "Could not create receiver thread\n",
			ret, sfsd->log_ctx, SFS_ERR);

	sfsd->thread_pool = sstack_thread_pool_create(1, 5, 30, &attr);
	SFS_LOG_EXIT((sfsd->thread_pool != NULL),
			"Could not create sfsd thread pool\n",
			ret, sfsd->log_ctx, SFS_ERR);

	sfsd->chunk_thread_pool = sstack_thread_pool_create(1, 2, 0, &attr);
	SFS_LOG_EXIT((sfsd->thread_pool != NULL),
			"Could not create sfsd chunk thread pool\n",
			ret, sfsd->log_ctx, SFS_ERR);

	sfs_log(sfsd->log_ctx, SFS_INFO, "Thread pool initialized\n");
	return 0;

}

static sstack_payload_t* get_payload(sstack_transport_t *transport,
		sstack_client_handle_t handle)
{
	sstack_payload_t *payload = NULL;
	ssize_t nbytes = 0;
	sstack_transport_ops_t *ops = &transport->transport_ops;

	payload = malloc(sizeof(*payload));
	SFS_LOG_EXIT((payload != NULL), "Allocate payload failed\n", NULL,
			transport->ctx, SFS_ERR);
	/* Read of the payload */
	nbytes = ops->rx(handle, sizeof(*payload), payload);
	sfs_log (transport->ctx, ((nbytes == 0) ? SFS_ERR : SFS_DEBUG),
			"Payload size: %d\n", nbytes);

	return payload;
}

int32_t send_payload(sstack_transport_t *transport,
		sstack_client_handle_t handle, sstack_payload_t *payload)
{
	int ret = -1;
	sstack_job_id_t job_id = 0;
	int priority = -1;
	sfs_job_t *job = NULL;

	// Parameter validation
	if (NULL == transport || handle == -1 || NULL == payload) {
		// Failure
		// Handle failure
		return -1;
	}
	job_id = payload->hdr.job_id;
	priority = payload->hdr.priority;
	job = (sfs_job_t *) payload->hdr.arg;

	// TODO
	// Get log_ctx. Needed for paramter validaion
	ret = sstack_send_payload(handle, payload, transport, job_id, job,
					priority, NULL);
	return ret;
}

static void* do_process_payload(void *param)
{
	struct handle_payload_params *h_param =
		(struct handle_payload_params *)param;
	sstack_payload_t *command = h_param->payload;
	sstack_payload_t *response = NULL;
	bds_cache_desc_t param_cache = h_param->cache_p;
	handle_command(command, &response, h_param->cache_arr,
		       h_param->sfsd, h_param->log_ctx);
	/* Free of the param cache */
	bds_cache_free(param_cache, h_param);

	/* Send of the response */
	if (send_payload(h_param->sfsd->transport,
				h_param->sfsd->handle, response)) {
		sfs_log (h_param->log_ctx, SFS_ERR, "Error sending payload\n");
	}

	/* Free off the response */
	bds_cache_free(h_param->cache_arr[PAYLOAD_CACHE_OFFSET], response);
	return NULL;
}

static void* do_receive_thread(void *param)
{
	sfsd_t *sfsd = (sfsd_t *)param;
	int32_t ret = 0;
	uint32_t mask = READ_BLOCK_MASK;
	sstack_payload_t *payload;
	struct handle_payload_params *handle_params;
	bds_cache_desc_t param_cache = NULL;
	/* Check for the handle, if it doesn't exist
	   simply exit */
	SFS_LOG_EXIT((sfsd->handle != 0),
			"Transport handle doesn't exist. Exiting..\n",
			NULL, sfsd->log_ctx, SFS_ERR);

	sfs_log(sfsd->log_ctx, SFS_DEBUG, "%s: %d \n", __FUNCTION__, __LINE__);

	param_cache = sfsd->caches[HANDLE_PARAM_OFFSET];

	while (1) {
		/* Check whether there is some command from the
		   sfs coming */
		ret = sfsd->transport->transport_ops.select(sfsd->handle, mask);
		if (ret != READ_NO_BLOCK) {
			/* Connection is down, wait for retry */
			//sleep (1);
			/* Select in this case is non-blocking. We could come out
			 * even when there is nothing to read. So, go back to select
			 */
			continue;
		}
		sfs_log(sfsd->log_ctx, SFS_DEBUG, "%s: %d \n", __FUNCTION__, __LINE__);
		payload = sstack_recv_payload(sfsd->handle,sfsd->transport, sfsd->log_ctx);
		/* After getting the payload, assign a thread pool from the
		   thread pool to do the job */
		sfs_log(sfsd->log_ctx, SFS_DEBUG, "%s: %d payload 0x%x\n",
				__FUNCTION__, __LINE__, payload);
#if 0
		if (payload != NULL) {
			/* Command could be
			   1) Chunk domain command
			   2) Standard NFS command */
			handle_params = bds_cache_alloc(param_cache);
			if (handle_params == NULL) {
				sfs_log(sfsd->log_ctx, SFS_ERR,
						"Failed to allocate %s",
						"handle_params\n");
				send_unsucessful_response(payload, sfsd);
				continue;
			}
			handle_params->payload = payload;
			handle_params->log_ctx = sfsd->log_ctx;
			handle_params->cache_arr =
				sfsd->caches;
			handle_params->cache_p = param_cache;
			handle_params->sfsd = sfsd;
			ret = sstack_thread_pool_queue(sfsd->thread_pool,
					do_process_payload, handle_params);
			sfs_log(sfsd->log_ctx,
				((ret == 0) ? SFS_DEBUG: SFS_ERR),
				"Job queue status: %d\n", ret);
		}
#endif
	}
}

void handle_command(sstack_payload_t *command, sstack_payload_t **response,
		bds_cache_desc_t *cache_arr, sfsd_t *sfsd, log_ctx_t *log_ctx)
{
	sfsd_storage_t *storage;
	char *path;
	int32_t ret = 0;
	switch(command->command)
	{
		/* sstack storage commands here */
		case SSTACK_ADD_STORAGE:
			storage = &command->storage;
			path = sfsd_add_chunk(sfsd->chunk, storage);
			if (path) {
				free(path);
			} else {
				ret = -EINVAL;
			}
			command->response_struct.command_ok = ret;
			*response = command;
			break;
		case SSTACK_UPDATE_STORAGE:
			storage = &command->storage;
			if (0 != (ret = sfsd_update_chunk(sfsd->chunk,
							storage))) {
				sfs_log(log_ctx, SFS_ERR, "Unable to update"
						"chunk\n");
			}
			command->response_struct.command_ok = ret;
			*response = command;
			break;
		case SSTACK_REMOVE_STORAGE:
			storage = &command->storage;
			if (0 != (ret = sfsd_remove_chunk(sfsd->chunk,
							storage))) {
				sfs_log(log_ctx, SFS_ERR, "Unable to remove"
						"chunk\n");
			}
			command->response_struct.command_ok = ret;
			*response = command;
			break;
		/* sstack nfs commands here */
		case NFS_GETATTR:
			*response = sstack_getattr(command, log_ctx);
			break;
		case NFS_SETATTR:
			*response = sstack_setattr(command, log_ctx);
			break;
		case NFS_LOOKUP:
			*response = sstack_lookup(command, log_ctx);
			break;
		case NFS_ACCESS:
			*response = sstack_access(command, log_ctx);
			break;
		case NFS_READLINK:
			*response = sstack_readlink(command, log_ctx);
			break;
		case NFS_READ:
			*response = sstack_read(command, sfsd,log_ctx);
			break;
		case NFS_WRITE:
			*response = sstack_write(command, sfsd, log_ctx);
			break;
		case NFS_CREATE:
			*response = sstack_create(command, sfsd, log_ctx);
			break;
		case NFS_MKDIR:
			*response = sstack_mkdir(command, log_ctx);
			break;
		case NFS_SYMLINK:
			*response = sstack_symlink(command, log_ctx);
			break;
		case NFS_MKNOD:
			*response = sstack_mknod(command, log_ctx);
			break;
		case NFS_REMOVE:
			*response = sstack_remove(command, log_ctx);
			break;
		case NFS_RMDIR:
			*response = sstack_rmdir(command, log_ctx);
			break;
		case NFS_RENAME:
			*response = sstack_rename(command, log_ctx);
			break;
		case NFS_LINK:
			*response = sstack_link(command, log_ctx);
			break;
		case NFS_READDIR:
			*response = sstack_readdir(command, log_ctx);
			break;
		case NFS_READDIRPLUS:
			*response = sstack_readdirplus(command, log_ctx);
			break;
		case NFS_FSSTAT:
			*response = sstack_fsstat(command, log_ctx);
			break;
		case NFS_FSINFO:
			*response = sstack_fsinfo(command, log_ctx);
			break;
		case NFS_PATHCONF:
			*response = sstack_pathconf(command, log_ctx);
			break;
		case NFS_COMMIT:
			*response = sstack_commit(command, log_ctx);
			break;
		case NFS_ESURE_CODE:
			*response = sstack_esure_code(command, sfsd, log_ctx);
			break;
		default:
			break;
	};

	(*response)->hdr.sequence = command->hdr.sequence;
	/* Send off response from here */
	sstack_send_payload(sfsd->handle, command, sfsd->transport,
						command->hdr.job_id, (sfs_job_t*)command->hdr.arg,
						command->hdr.priority, log_ctx);
	free_payload(sfsd_global_cache_arr,command);

}
