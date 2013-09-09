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
