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

#define SFSD_MAGIC 0x11101974
#include <stdint.h>
#define MAX_SFSD_CLIENTS 8
#define IPV4_ADDR_MAX 16
#define IPV6_ADDR_MAX 40
#define MAX_QUEUE_SIZE 1024

typedef uint64_t sfsd_client_handle_t;
typedef uint8_t sfsd_payload_t;

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
	TCPIP	= 1,
	IB	= 2,
	UDP	= 3,
	UNIX	= 4,
} sfsd_transport_type_t;

typedef enum {
	HIGH_PRIORITY = 1,
	MEDIUM_PRIORITY = 2,
	LOW_PRIORITY = 3,
	NUM_PRIORITY_MAX,
} sfs_prio_t;

typedef struct sfsd_transport {
	sfsd_transport_type_t  transport_type;
	union {
		struct tcp {
			int ipv4;		// 1 for ipv4 and 0 for ipv6
			union {
				char ipv4_addr[IPV4_ADDR_MAX];
				char ipv6_addr[IPV6_ADDR_MAX];
			};
			int port;
		} tcp;
	};
} sfsd_transport_t;

/*
 *  init() is supposed to establish a connection and retrun client handle
 *  Client handle is socket fd i case of TCPIP
 */
typedef struct sfsd_transport_ops {
	int (*init) (sfsd_transport_t *);
	int (*tx) (sfsd_client_handle_t , int , char *); 
	int (*rx) (sfsd_client_handle_t , int, char *);
} sfsd_transport_ops_t;

typedef struct sfsd {
	sfsd_transport_t sfsd_transport;
	sfsd_transport_ops_t sfsd_transport_ops;
	sfsd_client_handle_t sfsd_handle;
} sfsd_t;

typedef struct job {
	int version; 
	sfsd_job_type_t job_type;
	int num_clients; // Number of valid clients
	sfsd_t sfsd_t[MAX_SFSD_CLIENTS];
	sfsd_job_status_t job_status[MAX_SFSD_CLIENTS]; // Status of each client
	int payload_len;
	int priority; /* Priority of the job */
	sfsd_payload_t	payload[0];
} job_t;

/* 
 * Multiple jobs queues in SFS, one for each priority level 
 * exists. Scheduler threads each per sfsds pick one payload
 * at a time and send of to one of the actual sfsd.
 */
typedef struct job_queue {
	int priority; /* Priority of job queue */
	sfsd_payload_t *payload[MAX_QUEUE_SIZE]; /* The array of payloads */
}

typedef enum {
	INITIALIZING 	= 1, // sfs sent a request to spawn sfsd
	RUNNING		= 2, // Handshake between sfs and sfsd is up
	REACHABLE	= 3, // Heartbeat successful
	UNREACHABLE	= 4, // Heartbeat dead. Could be n/w or sfsd
	DECOMMISSIONED	= 5, // Node running sfsd is decommissioned. A temp state
} sfsd_state_t;


// Different types of payloads to be defined here
// TBD


#endif // __SSTACK_JOBS_H_
