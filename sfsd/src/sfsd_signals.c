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
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <sstack_sfsd.h>

/* PRIVATE DEFINITIONS */
static void handle_sighup(int);


/* PUBLIC FUNCTIONS */
int32_t register_signals(void)
{
	int ret = 0;

	signal(SIGHUP, handle_sighup);

	return ret;
}

static void handle_sighup(int signum)
{
	fprintf(stderr, "Signal no. %d\n", signum);
}
