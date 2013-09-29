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

#ifndef __SSTACK_STORAGE_H_
#define __SSTACK_STORAGE_H_

#define MAX_MOUNT_POINT_LEN 16
#include <sstack_types.h>
#include <sstack_transport.h>

typedef struct sfsd_storage {
	char path[PATH_MAX];
	char mount_point[MAX_MOUNT_POINT_LEN];
	uint32_t weight;
	uint64_t nblocks;
	sfs_protocol_t  protocol;
	uint64_t num_chunks_written;
	sstack_address_t address;
} sfsd_storage_t;
#endif /*__SSTACK_STORAGE_H_ */
