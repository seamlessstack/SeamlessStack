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

#endif // __SSTACK_SERDES_H_	
