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

static void* do_receive_thread (void *params)
{
	return 0;
}
static int32_t sfsd_create_receiver_thread(sfsd_local_t *sfsd)
{
	int32_t ret = 0;
	pthread_attr_t attr;

	pthread_attr_init(&attr);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

	ret = pthread_create(&sfsd->receiver_thread, &attr,
			do_receive_thread,sfsd->receiver_params);
	SFS_LOG_EXIT((ret == 0), "Receiver thread create failed", ret,
			sfsd->log_ctx, SFS_ERR);

	return 0;
}

int32_t init_thread_pool(sfsd_local_t *sfsd)
{
	int32_t ret = 0;
	pthread_attr_t attr;
	sstack_thread_pool_t *thread_pool;

	pthread_attr_init(&attr);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

	ret = sfsd_create_receiver_thread(sfsd);
	SFS_LOG_EXIT((ret == 0), "Could not create receiver thread",
			ret, sfsd->log_ctx, SFS_ERR);

	thread_pool = sstack_thread_pool_create(1, 5, 30, &attr);
	SFS_LOG_EXIT((thread_pool != NULL), "Could not create sfsd thread pool",
			ret, sfsd->log_ctx, SFS_ERR);

	/* O.K the thread stuff is done.. Wait for incoming connections */

	return 0;
	
}

