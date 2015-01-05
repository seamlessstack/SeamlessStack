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

#ifndef __SSTACK_SERDES_H_
#define __SSTACK_SERDES_H_

#include <stdint.h>
#include <sstack_jobs.h>
#include <sstack_transport.h>
#include <sstack_types.h>

#define SERDES_PAYLOAD_CACHE_IDX 0
#define SERDES_DATA_4K_CACHE_IDX 1
#define SERDES_DATA_64K_CACHE_IDX 2
#define SERDES_NUM_CACHES 3

extern int32_t sstack_serdes_init(bds_cache_desc_t **cache_array);
extern char * sstack_command_stringify(sstack_command_t command);
/*
 * sstack_send_payload - Function that serializes payload and sends
 *		it across to the reciver
 */

extern int sstack_send_payload(sstack_client_handle_t handle,
				sstack_payload_t *payload, sstack_transport_t *transport,
				sstack_job_id_t job_id, sfs_job_t *job,
				int priority, log_ctx_t *ctx);

/*
 * sstack_recv_payload - Function that receives a payload from transport
 * 		and deserializes it before handing it over to the receiver
 */

extern sstack_payload_t * sstack_recv_payload(sstack_client_handle_t handle,
				sstack_transport_t *transport, log_ctx_t *ctx);

sstack_payload_t *sstack_create_payload(sstack_command_t cmd);
void sstack_free_payload(sstack_payload_t *payload);
#endif // __SSTACK_SERDES_H_	
