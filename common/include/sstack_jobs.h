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

#ifndef __SSTACK_JOBS_H_
#define __SSTACK_JOBS_H_

#include <pthread.h>
#include <sstack_types.h>
#include <sstack_transport.h>
#include <sstack_nfs.h>
#include <sstack_storage.h>
#include <sstack_log.h>
#include <sstack_db.h>
#include <sstack_thread_pool.h>
#include <sstack_nfs.h>
#include <bds_types.h>
#include <bds_slab.h>
#include <bds_list.h>

#define MAX_SFSD_CLIENTS 8 // Max clients per request
#define IPV4_ADDR_MAX 16
#define IPV6_ADDR_MAX 40
#define MAX_QUEUE_SIZE 1024
#define MAX_EXTENT_SIZE 65536 /* (64 * 1024 bytes) */
#define SFS_JOB_VERSION 1
#define MAX_OUTSTANDING_JOBS 2147483647 // 2^31 -1

// Cache indexes
/* To add a new cache define a 
   new offset here and add an
   entry to the sstack_create_cache
   function */
#define PAYLOAD_CACHE_OFFSET 1
#define HANDLE_PARAM_OFFSET 2
#define INODE_CACHE_OFFSET 3
#define DATA4K_CACHE_OFFSET 4
#define DATA64K_CACHE_OFFSET 5
#define MAX_CACHE_OFFSET DATA64K_CACHE_OFFSET
/* Forward declaration */
struct sfs_chunk_domain;
typedef struct sfs_chunk_domain sfs_chunk_domain_t;

typedef enum {
	SFSD_JOB_INIT = -1,
	SFSD_HANDSHAKE	= 0,
	SFSD_IO	= 1,
	SFSD_MAX_JOB_TYPE = 1,
} sstack_job_type_t;

typedef enum {
	JOB_INIT = -1,
	JOB_STARTED	= 0,
	JOB_COMPLETE = 1,
	JOB_ABORTED	= 2,
	JOB_FAILED	= 3,
	MAX_JOB_STATUS = 3,
} sstack_job_status_t;

typedef enum {
	INVALID_PRIORITY = -1,
	HIGH_PRIORITY = 0,
	MEDIUM_PRIORITY = 1,
	LOW_PRIORITY = 2,
	NUM_PRIORITY_MAX = 2,
} sfs_prio_t;

/*
 * NOTE
 * arg field is a back pointer to the job structure. This is needed to get the
 * job structure to send the response back to the calling thread.
 * This can be accomplished by having a separate lookup. But that is an
 * overhead which can be avoided.
 * sfsd can also use sstack_send_payload(). If the original request is from
 * sfs, then sfsd can use transport.transport_hdr.arg as job pointer. If sfsd
 * needs to originate a request, then we can use 0xffffffff-ffffffff as this
 * argument. This is an optimization.
 */
typedef struct sstack_payload_hdr {
	uint32_t sequence;
	uint32_t payload_len;
	sstack_job_id_t job_id;
	int	priority;
	uint64_t arg;	// Contains back pointer to job strcuture
} sstack_payload_hdr_t;

typedef struct sstack_payload {
	sstack_payload_hdr_t hdr;
	sstack_command_t command;
	union {
		sfsd_storage_t storage;
		sstack_nfs_command_struct command_struct;
		sstack_nfs_response_struct response_struct;
	};
} sstack_payload_t;

typedef struct sfsd {
	/* To be used in sfs and sfsd */
	log_ctx_t *log_ctx;
	sstack_transport_t *transport;
	sstack_client_handle_t handle;
	bds_int_list_t list; // This is for queueing sfsd to sfsd pool
	/* To be used in sfsd;
	   Undefined if accessed in sfs */

	/* The chunk domain which this sfsd
	   controls */
	sfs_chunk_domain_t *chunk;
	pthread_t receiver_thread;
	void *receiver_params;
	db_t *db;
	char sfs_addr[IPV4_ADDR_MAX];
	sstack_thread_pool_t *thread_pool;
	sstack_thread_pool_t *chunk_thread_pool;
	bds_cache_desc_t *caches;
	uint32_t sfs_pool_wgt;
} sfsd_t;

typedef struct sfs_job {
	int version;
	sstack_job_type_t job_type;
	sstack_job_id_t	id;
	int num_clients; // Number of valid clients
	sfsd_t *sfsds[MAX_SFSD_CLIENTS];
	sstack_job_status_t job_status[MAX_SFSD_CLIENTS]; // Status of each client
	int payload_len;
	int priority; /* Priority of the job */
	bds_int_list_t wait_list;
	sstack_payload_t *payload;
} sfs_job_t;

/*
 * Multiple jobs queues in SFS, one for each priority level
 * exists. Scheduler threads each per sfsds pick one payload
 * at a time and send of to one of the actual sfsd.
 */
typedef struct sfs_job_queue {
	int priority; /* Priority of job queue */
	bds_int_list_t list;
	pthread_spinlock_t lock;
} sfs_job_queue_t;

/*
 * free_payload - Free the payload structure back to slab
 *
 * payload - Valid payload pointer. Should be non-NULL
 */

static inline void
free_payload(bds_cache_desc_t *caches, sstack_payload_t *payload)
{
	// Parameter validation
	if (NULL == payload) {
		return;
	}
	if (payload->command == NFS_READ_RSP &&
			payload->response_struct.read_resp.data.data_buf != NULL) {
		bds_cache_free(caches[DATA64K_CACHE_OFFSET],
			payload->response_struct.read_resp.data.data_buf);
	} else if (payload->command == NFS_WRITE &&
			payload->command_struct.write_cmd.data.data_buf != NULL) {
		bds_cache_free(caches[DATA64K_CACHE_OFFSET],
			payload->command_struct.write_cmd.data.data_buf);
	}

	bds_cache_free(caches[PAYLOAD_CACHE_OFFSET], (void *) payload);
}

#endif // __SSTACK_JOBS_H_
