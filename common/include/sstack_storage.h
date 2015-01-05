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
