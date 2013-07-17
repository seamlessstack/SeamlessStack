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

typedef uint64_t sfsd_client_handle_t;
typedef uint8_t sfsd_payload_t;

typedef enum {
	SFSD_HANDSHAKE	= 1,
	SFSD_NFS		= 2,
} sfsd_job_type_t;


typedef struct job {
	int	version;
	sfsd_client_handle_t client_handle;	// Client identifier
	sfsd_job_type_t job_type;
	int	payload_len;
	sfsd_payload_t	payload;
} job_t;

#endif // __SSTACK_JOBS_H_
