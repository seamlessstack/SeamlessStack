/*
 * Copyright (C) 2014 SeamlessStack
 *
 *  This file is part of SeamlessStack distributed file system.
 * 
 * SeamlessStack distributed file system is free software: you can redistribute
 * it and/or modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 * 
 * SeamlessStack distributed file system is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with SeamlessStack distributed file system. If not,
 * see <http://www.gnu.org/licenses/>.
 */

#ifndef _SSTACK_SFSD_H_
#define _SSTACK_SFSD_H_

#include <sstack_log.h>
#include <sstack_jobs.h>
#include <sstack_transport.h>
#include <sstack_thread_pool.h>
#include <bds_slab.h>


/** Structure to pass multiple
 *  parameters to a function
 **/
struct handle_payload_params {
	sstack_payload_t *payload;
	bds_cache_desc_t *cache_arr;
	log_ctx_t *log_ctx;
	/* Pointer to cache descriptor of self */
	bds_cache_desc_t cache_p;
	sfsd_t	*sfsd;
};
/**
 * Register all the relevant 
 * signals in this funtion
 **/
int32_t register_signals(sfsd_t *);

/** 
 * Create the thread pool
 **/

int32_t init_thread_pool(sfsd_t *);

/** 
  * initialize transport
 **/
int32_t init_transport(sfsd_t *);

/**
 * Run the actual SFS daemon
 **/
void run_daemon_sfsd(sfsd_t *);


#endif /* _STACK_SFSD_H_ */
