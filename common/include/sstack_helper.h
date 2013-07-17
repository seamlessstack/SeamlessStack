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

#ifndef __SSTACK_HELPER_H_
#define __SSTACK_HELPER_H_

#include <stdarg.h>
#include <string.h>
#include <sstack_log.h>

static inline char *
get_opt_str(const char *arg, char *opt_name)
{
	char *str = index(arg, '=');

	ASSERT((str), "Invalid parameter specified. Exiting...", 0, 0, 0);
	if (!str)
		exit(-1);
	
	str ++;
	str = strdup(str);
	ASSERT((str), "strdup failed. Exiting...", 0, 0, 0);
	if (!str)
		exit(-1);

	return str;
}

#endif //__SSTACK_HELPER_H_
