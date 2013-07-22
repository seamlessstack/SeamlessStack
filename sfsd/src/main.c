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

#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sstack_log.h>
#include <sstack_sfsd.h>

/* sstack log directory */
char sstack_log_directory[PATH_MAX];
log_ctx_t *sfsd_ctx = NULL;

int main(int argc, char **argv)
{

	log_ctx_t *ctx = NULL;
	int log_ret = 0;

	/* Set up the log directory */
	if (argc == 1) {
		fprintf (stdout, "Usage: %s <log directory>\n", argv[0]);
	} else {
		strncpy(sstack_log_directory, argv[1], PATH_MAX);
	}
	
	/* initialize logging */
	ctx = sfs_create_log_ctx();
	ASSERT((NULL != ctx), "Log create failed", 1, 1, 0); 
	log_ret = sfs_log_init(ctx, SFS_INFO, "sfsd");
	ASSERT((0 == log_ret), "Log init failed", 1, 1, 0);

	sfsd_ctx = ctx;

	/* Register signals */
	ASSERT ((0 == register_signals()),
			"Signal regisration failed", 1, 1, 0);
#if 0
	/* Initialize transport */
	init_transport();
#endif
	/* Initialize thread pool */
	init_thread_pool();

	/* Daemonize */
	//daemon(0, 0);

	/* Run the sfsd daemon */
	run_daemon_sfsd();

	/* Control never returns */
	return 0;
}
