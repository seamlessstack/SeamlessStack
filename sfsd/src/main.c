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
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sstack_log.h>
#include <sstack_transport.h>
#include <sstack_sfsd.h>
#include <bds_slab.h>

/* sstack log directory */
char sstack_log_directory[PATH_MAX];
static sfsd_local_t sfsd;

int main(int argc, char **argv)
{

	log_ctx_t *ctx = NULL;
	int log_ret = 0;

	/* Set up the log directory */
	if (argc == 1) {
		fprintf (stdout, "Usage: %s <log directory> <sfs address>\n",
				argv[0]);
	} else {
		strncpy(sstack_log_directory, argv[1], PATH_MAX);
	}
	memset(&sfsd, 0, sizeof(sfsd));
	
	/* Get the server address from the command line also */
	strcpy(sfsd.sfs_addr, argv[2]);

	/* initialize logging */
	ctx = sfs_create_log_ctx();
	ASSERT((NULL != ctx), "Log create failed", 1, 1, 0); 
	log_ret = sfs_log_init(ctx, SFS_DEBUG, "sfsd");
	ASSERT((0 == log_ret), "Log init failed", 1, 1, 0);

	sfsd.log_ctx = ctx;

	/* Register signals */
	ASSERT ((0 == register_signals(&sfsd)),
			"Signal regisration failed", 1, 1, 0);

	/* Mount devices to appropriate mount points */
	//init_storage_devs(&sfsd);

	/* Initialize transport */
	init_transport(&sfsd);

	/* Initialize thread pool */
	init_thread_pool(&sfsd);

	/* Daemonize */
	//daemon(0, 0);

	while(1);
	/* Run the sfsd daemon */
	run_daemon_sfsd(&sfsd);

	/* Control never returns */
	return 0;
}
