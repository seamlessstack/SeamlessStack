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
#include <sstack_jobs.h>

static void* do_receive_thread (void *params);

static int32_t sfsd_create_receiver_thread(sfsd_local_t *sfsd)
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

int32_t init_thread_pool(sfsd_local_t *sfsd)
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
	sstack_payload_t *command = (sstack_payload_t *)param;
	sstack_payload_t *response = NULL;
	uint32_t payload_id = command->payload_id;


	return 0;
}

static void* do_receive_thread(void *param)
{
	sfsd_local_t *sfsd = (sfsd_local_t *)param;
	int32_t ret = 0;
	uint32_t mask = READ_BLOCK_MASK;
	sstack_payload_t *payload;
	/* Check for the handle, if it doesn't exist
	   simply exit */
	SFS_LOG_EXIT((sfsd->handle != 0), 
			"Transport handle doesn't exist. Exiting..",
			NULL, sfsd->log_ctx, SFS_ERR);
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
			ret = sstack_thread_pool_queue(sfsd->thread_pool,
					do_process_payload, payload);
			sfs_log(sfsd->log_ctx,
					((ret == 0) ? SFS_DEBUG: SFS_ERR),
					"Job queue status: %d", ret);
		}
	}
}
