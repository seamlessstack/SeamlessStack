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

/* ===================== PRIVATE DECLARATIONS ============================ */
struct handle_payload_params {
	sstack_payload_t *payload;
	bds_cache_desc_t *cache_arr;
	log_ctx_t *log_ctx;
	/* Pointer to cache descriptor of self */
	bds_cache_desc_t cache_p;
};
static void* do_receive_thread (void *params);
static void handle_command(sstack_payload_t *command,
		sstack_payload_t **response,
		bds_cache_desc_t cache_arr[2], log_ctx_t *log_ctx);

/* ====================+++++++++++++++++++++++++++++++++==================*/

static int32_t sfsd_create_receiver_thread(sfsd_t *sfsd)
{
	int32_t ret = 0;
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

	ret = pthread_create(&sfsd->receiver_thread, &attr,
			do_receive_thread, sfsd);
	SFS_LOG_EXIT((ret == 0), "Receiver thread create failed", ret,
			sfsd->log_ctx, SFS_ERR);

	sfs_log(sfsd->log_ctx, SFS_INFO, "Receive thread creation successful");
	return 0;
}

int32_t init_thread_pool(sfsd_t *sfsd)
{
	int32_t ret = 0;
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

	ret = sfsd_create_receiver_thread(sfsd);
	SFS_LOG_EXIT((ret == 0), "Could not create receiver thread",
			ret, sfsd->log_ctx, SFS_ERR);

	sfsd->thread_pool = sstack_thread_pool_create(1, 5, 30, &attr);
	SFS_LOG_EXIT((sfsd->thread_pool != NULL),
			"Could not create sfsd thread pool",
			ret, sfsd->log_ctx, SFS_ERR);

	sfs_log(sfsd->log_ctx, SFS_INFO, "Thread pool initialized");
	return 0;
	
}

static sstack_payload_t* get_payload(sstack_transport_t *transport,
		sstack_client_handle_t handle)
{
	sstack_payload_t *payload = NULL;
	ssize_t nbytes = 0;
	sstack_transport_ops_t *ops = &transport->transport_ops;

	payload = malloc(sizeof(*payload));
	SFS_LOG_EXIT((payload != NULL), "Allocate payload failed", NULL, 
			transport->ctx, SFS_ERR);
	/* Read of the payload */ 
	nbytes = ops->rx(handle, sizeof(*payload), payload);
	sfs_log (transport->ctx, ((nbytes == 0) ? SFS_ERR : SFS_DEBUG), 
			"Payload size: %d\n", nbytes);

	return payload;
}

static void* do_process_payload(void *param)
{
	struct handle_payload_params *h_param =
		(struct handle_payload_params *)param;
	sstack_payload_t *command = h_param->payload;
	sstack_payload_t *response = NULL;
	bds_cache_desc_t param_cache = h_param->cache_p;

	handle_command(command, &response, h_param->cache_arr, h_param->log_ctx);
	/* Free of the param cache */
	bds_cache_free(param_cache, h_param);

	/* Send of the response */
	

	return 0;
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
			"Transport handle doesn't exist. Exiting..",
			NULL, sfsd->log_ctx, SFS_ERR);
	/* Create the caches */
	ret = bds_cache_create("payload-cache", sizeof(sstack_payload_t), 0,
			NULL, NULL,
			&sfsd->payload_cache_arr[PAYLOAD_CACHE_OFFSET]);

	SFS_LOG_EXIT((ret == 0),
			"Could not create payload cache. Exiting..",
			NULL, sfsd->log_ctx, SFS_ERR);

	ret = bds_cache_create("data-cache", sizeof(sstack_payload_t), 0,
			NULL, NULL,
			&sfsd->payload_cache_arr[DATA_CACHE_OFFSET]);

	SFS_LOG_EXIT((ret == 0),
			"Could not create data cache. Exiting..",
			NULL, sfsd->log_ctx, SFS_ERR);

	ret = bds_cache_create("param-cache", sizeof(*handle_params), 0,
			NULL, NULL,
			&sfsd->payload_cache_arr[HANDLE_PARAM_OFFSET]);

	SFS_LOG_EXIT((ret == 0),
			"Could not create param cache. Exiting..",
			NULL, sfsd->log_ctx, SFS_ERR);
	param_cache = sfsd->payload_cache_arr[HANDLE_PARAM_OFFSET];

	sfs_log(sfsd->log_ctx, SFS_DEBUG, "All cache creations are successful");

	while (1) {
		/* Check whether there is some command from the
		   sfs coming */
		ret = sfsd->transport->transport_ops.select(sfsd->handle, mask);
		if (ret != READ_NO_BLOCK) {
			/* Connection is down, wait for retry */
			sleep (1);
		}
		payload = get_payload(sfsd->transport, sfsd->handle);
		/* After getting the payload, assign a thread pool from the
		   thread pool to do the job */
		if (payload != NULL) {
			handle_params = bds_cache_alloc(param_cache);
			if (handle_params == NULL) {
				sfs_log(sfsd->log_ctx, SFS_ERR, 
						"Failed to allocate %s",
						"handle_params");
				/* TODO: need to send an error response here */
				continue;
			}

			handle_params->payload = payload;
			handle_params->log_ctx = sfsd->log_ctx;
			handle_params->cache_arr = sfsd->payload_cache_arr;
			handle_params->cache_p = param_cache;
			ret = sstack_thread_pool_queue(sfsd->thread_pool,
					do_process_payload, handle_params);
			sfs_log(sfsd->log_ctx,
					((ret == 0) ? SFS_DEBUG: SFS_ERR),
					"Job queue status: %d", ret);
		}
	}
}

void handle_command(sstack_payload_t *command, sstack_payload_t **response,
		bds_cache_desc_t cache_arr[2], log_ctx_t *log_ctx)
{
	switch(command->command)
	{
		case NFS_GETATTR:
			*response = sstack_getattr(command, cache_arr, log_ctx);
			break;
		case NFS_SETATTR:
			*response = sstack_setattr(command, cache_arr, log_ctx);
			break;
		case NFS_LOOKUP:
			*response = sstack_lookup(command, cache_arr, log_ctx);
			break;
		case NFS_ACCESS:
			*response = sstack_access(command, cache_arr, log_ctx);
			break;
		case NFS_READLINK:
			*response = sstack_readlink(command, cache_arr,
					log_ctx);
			break;
		case NFS_READ:
			*response = sstack_read(command, cache_arr, log_ctx);
			break;
		case NFS_WRITE:
			*response = sstack_write(command, cache_arr, log_ctx);
			break;
		case NFS_CREATE:
			*response = sstack_create(command, cache_arr, log_ctx);
			break;
		case NFS_MKDIR:
			*response = sstack_mkdir(command, cache_arr, log_ctx);
			break;
		case NFS_SYMLINK:
			*response = sstack_symlink(command, cache_arr, log_ctx);
			break;
		case NFS_MKNOD:
			*response = sstack_mknod(command, cache_arr, log_ctx);
			break;
		case NFS_REMOVE:
			*response = sstack_remove(command, cache_arr, log_ctx);
			break;
		case NFS_RMDIR:
			*response = sstack_rmdir(command, cache_arr, log_ctx);
			break;
		case NFS_RENAME:
			*response = sstack_rename(command, cache_arr, log_ctx);
			break;
		case NFS_LINK:
			*response = sstack_link(command, cache_arr, log_ctx);
			break;
		case NFS_READDIR:
			*response = sstack_readdir(command, cache_arr, log_ctx);
			break;
		case NFS_READDIRPLUS:
			*response = sstack_readdirplus(command, cache_arr,
					log_ctx);
			break;
		case NFS_FSSTAT:
			*response = sstack_fsstat(command, cache_arr,
					log_ctx);
			break;
		case NFS_FSINFO:
			*response = sstack_fsinfo(command, cache_arr, log_ctx);
			break;
		case NFS_PATHCONF:
			*response = sstack_pathconf(command, cache_arr,
					log_ctx);
			break;
		case NFS_COMMIT:
			*response = sstack_commit(command, cache_arr, log_ctx);
			break;
		default:
			break;
	};
	
	(*response)->hdr.sequence = command->hdr.sequence;
}
