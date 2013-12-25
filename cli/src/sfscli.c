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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/errno.h>
#include <sfs.h>

/* ====================== FUNCTION DECLARATIONS ======================= */
static int32_t sfscli_connect(char *address, char *port);
static int32_t process_args(int32_t args, char *argv[], int32_t sockfd);



/* ====================== FUNCTION DEFINITIONS ======================== */
void usage(char *progname)
{
	fprintf(stderr, "Usage\n");
}

int main(int argc, char *argv[])
{
	char *sfs_addr;
	char *sfs_port;
	int32_t ret = 0;
	int32_t sockfd;

	sfs_addr = getenv("SFS_ADDR");
	sfs_port = getenv("SFS_PORT");

	if (sfs_addr == NULL || sfs_port == NULL) {
		fprintf(stderr, "SFS_ADDR or SFS_PORT not set\n");
		return -EINVAL;
	}

	if ((sockfd = sfscli_connect(sfs_addr, sfs_port)) < 0) {
		fprintf(stderr, "SFS not running on %s:%s, Please check settings\n",
				sfs_addr, sfs_port);
		return -EINVAL;
	}

	/* Now that we are connected we can process inputs from command line */
	ret = process_args(argc, argv, sockfd);

	return ret;
}


static int32_t sfscli_connect(char *address, char *port)
{
	printf ("Connecting to %s:%s...\n", address, port);
	return 0;
}

int32_t process_args(int32_t argc, char *argv[], int32_t sockfd)
{
	if (argc == 1) {
		usage(argv[0]);
		return -EINVAL;
	}

	if (!strcmp(argv[0], "storage")) {
		fprintf (stdout, "storage command\n");
	} else if (!strcmp(argv[0], "policy")) {
		printf ("policy command\n");
	} else if (!strcmp(argv[0], "sfsd")) {
		printf ("sfsd command\n");
	} else if (!strcmp(argv[0], "key")) {
		printf ("key command\n");
	} else if (!strcmp(argv[0], "license")) {
		printf ("license command\n");
	} else {
		printf ("invalid command\n");
	}
	return 0;
}
