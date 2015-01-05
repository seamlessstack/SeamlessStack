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

#ifndef __SSTACK_POLICY_H_
#define __SSTACK_POLICY_H_

#include <stdint.h>

// This file will contain policy data structures
// common between sfs and sfsd. Other parts will
// go into a separate header file under $ROOT/policy

// Define various policies here
typedef struct policy {
	uint8_t p_qoslevel; //  self explanatory
	uint8_t p_ishidden:1; // Not shown upon readdir()
	uint8_t p_isstriped:1; // If 0, lies as a whole file.
	uint64_t p_extentsize; // Size of each extent if striped in 4KiB blocks
	struct policy *next;
	// Add/modify/delete more policies
} policy_t;

//TBD
// A place holder for compilation

static policy_t default_policy = { 1, 0, 0, 0, NULL};


#endif // __SSTACK_POLICY_H_
