/*************************************************************************
 * sstack_threads.h - Thread pool handling data structures
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

#ifndef _SSTACK_THREADS_H_
#define _SSTACK_THREADS_H_

#define THREAD_NAME_MAX 32

/* The thread structure */
typedef struct sstack_thread
{
	/* Human readable description of the thread */
	char thread_name[THREAD_NAME_MAX]; 
	pthread_t thread;
	/* Attributes to the thread */
	pthread_attr_t attr;
	/* The entry function */
	void *(*start_routine)(void *);
	/* Arguments to the entry function */
	void *args;
} sstack_thread_t;

/* Thread pool structure. Any enitity interested in 
   creating a set of threads should fill in the 
   thread pool structure providing information for each of the 
   threads */
typedef struct sstack_thread_pool
{
	int32_t num_threads;
	sstack_thread_t threads[0];
} sstack_thread_pool_t;

/* =================== PUBLIC ACCESSOR FUNCTIONS ===================== */

void sstack_thread_pool_init(sstack_thread_pool_t *pool, int32_t num_threads);
int32_t sstack_thread_pool_create(sstack_thread_pool_t *pool, int32_t num_threads);
void sstack_thread_pool_destroy(
		sstack_thread_pool_t *pool, int32_t num_threads);

#endif /* _SSTACK_THREADS_H_ */
