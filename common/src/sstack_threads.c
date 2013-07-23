/*************************************************************************
 * 
 * sstack_thread.c - Functions for handling thread pool implementation.
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
#include <pthread.h>
#include <string.h>
#include <sstack_threads.h>

void sstack_thead_pool_init(sstack_thread_pool_t *pool, int32_t num_threads)
{
	int32_t i = 0;
	/* Initialize the thread pool direcory */
	memset(pool, 0, (sizeof(sstack_thread_pool_t)
				+ (num_threads * sizeof(sstack_thread_t))));
	/* Set up proper attributes for the threads in the pool */
	for(i = 0; i < num_threads; ++i)
	{
		pthread_attr_init(&pool->threads[i].attr);
		/* Set system scope for the threads */
		pthread_attr_setscope(&pool->threads[i].attr,
				PTHREAD_SCOPE_SYSTEM);
		/* Other attributes could be put as and when necessary */
	}

	return;
}


int32_t sstack_thread_pool_create(sstack_thread_pool_t *pool,
		int32_t num_threads)
{
	int32_t i = 0;
	int32_t ret = 0;

	/* Create the threads now */
	for(i = 0; i < num_threads; ++i)
	{
		if (pool->threads[i].start_routine == NULL)
			continue;
		ret = pthread_create(&pool->threads[i].thread,
				&pool->threads[i].attr,
				pool->threads[i].start_routine,
				pool->threads[i].args);
		if (ret != 0) {
			sstack_thread_pool_destroy(pool, i+1);
			break;
		}
	}
	return ret;
}

void sstack_thread_pool_destroy(sstack_thread_pool_t *pool, int32_t num_threads)
{
	/* Need to think about the implementation */
}
