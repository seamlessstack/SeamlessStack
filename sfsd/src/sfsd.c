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

#include <stdint.h>
#include <pthread.h>
#include <sstack_log.h>
#include <sstack_transport.h>
#include <sstack_sfsd.h>

extern log_ctx_t *sfsd_ctx;
void run_daemon_sfsd(sfsd_local_t *sfsd)
{
	sleep (2);
	sfs_log(sfsd->log_ctx, SFS_INFO, "%s", "Daemon started");
}
