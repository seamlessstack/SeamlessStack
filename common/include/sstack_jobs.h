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

#include <sstack_types.h>
#include <sstack_transport.h>

#define SFSD_MAGIC 0x11101974
#define MAX_SFSD_CLIENTS 8
#define IPV4_ADDR_MAX 16
#define IPV6_ADDR_MAX 40
#define MAX_QUEUE_SIZE 1024
#define MAX_EXTENT_SIZE 65536 /* (64 * 1024 bytes) */

#include <sstack_nfs.h>

typedef enum {
	SFSD_HANDSHAKE	= 1,
	SFSD_NFS	= 2,
} sfsd_job_type_t;

typedef enum {
	JOB_STARTED	= 1,
	JOB_COMPLETE	= 2,
	JOB_ABORTED	= 3,
	JOB_FAILED	= 4,
} sfsd_job_status_t;

typedef enum {
	HIGH_PRIORITY = 1,
	MEDIUM_PRIORITY = 2,
	LOW_PRIORITY = 3,
	NUM_PRIORITY_MAX,
} sfs_prio_t;

typedef struct sstack_payload {
	uint32_t payload_id;
	sstack_nfs_command_t command;
	union {
		sstack_nfs_command_struct command_struct;
		sstack_nfs_result_struct  result_struct;
	};
} sstack_payload_t;
	
typedef struct sfsd {
	sstack_transport_t sfsd_transport;
	sstack_client_handle_t sfsd_handle;
} sfsd_t;

typedef struct job {
	int version; 
	sfsd_job_type_t job_type;
	int num_clients; // Number of valid clients
	sfsd_t sfsd_t[MAX_SFSD_CLIENTS];
	sfsd_job_status_t job_status[MAX_SFSD_CLIENTS]; // Status of each client
	int payload_len;
	int priority; /* Priority of the job */
	sstack_payload_t	payload[0];
} job_t;

/* 
 * Multiple jobs queues in SFS, one for each priority level 
 * exists. Scheduler threads each per sfsds pick one payload
 * at a time and send of to one of the actual sfsd.
 */
typedef struct job_queue {
	int priority; /* Priority of job queue */
	sstack_payload_t *payload[MAX_QUEUE_SIZE]; /* The array of payloads */
} job_queue_t;

typedef enum {
	INITIALIZING 	= 1, // sfs sent a request to spawn sfsd
	RUNNING		= 2, // Handshake between sfs and sfsd is up
	REACHABLE	= 3, // Heartbeat successful
	UNREACHABLE	= 4, // Heartbeat dead. Could be n/w or sfsd
	DECOMMISSIONED	= 5, // Node running sfsd is decommissioned. A temp state
} sfsd_state_t;

#endif // __SSTACK_JOBS_H_
