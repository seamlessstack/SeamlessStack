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
#ifndef _SSTACK_SFSD_H_
#define _SSTACK_SFSD_H_

#include <sstack_log.h>
#include <sstack_transport.h>
#include <sstack_thread_pool.h>
/**
 * Contains all necessary information
 * for sfsd operation
 **/
typedef struct sfsd_local
{
	log_ctx_t *log_ctx;
	sstack_transport_t *transport;
	pthread_t receiver_thread;
	void *receiver_params;
	char sfs_addr[IPV4_ADDR_MAX];
	sstack_client_handle_t handle;
} sfsd_local_t;

/**
 * Register all the relevant 
 * signals in this funtion
 **/
int32_t register_signals(sfsd_local_t *);

/** 
 * Create the thread pool
 **/

int32_t init_thread_pool(sfsd_local_t *);

/**
 * Run the actual SFS daemon
 **/
void run_daemon_sfsd(sfsd_local_t *);


#endif /* _STACK_SFSD_H_ */
