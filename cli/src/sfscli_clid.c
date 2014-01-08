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
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sfscli.h>
#include <sfscli_clid.h>

#define INACTIVITY_INTERVAL_S 180 /* 3 minutes (3 * 60) */
#define INACTIVITY_INTERVAL_US 0

/* ===================== Function Declatations ============================ */
static int32_t clid_validate_params(char *clid_name,
									char *clid_port_str, char *clid_addr_str,
									char *sfs_port_str, char *sfs_addr_str,
									uint16_t *clid_port, in_addr_t *clid_addr,
									uint16_t *sfs_port, in_addr_t *sfs_sddr);
static int32_t clid_start(in_addr_t clid_addr, uint16_t clid_port);

static int32_t clid_connect_sfs(int32_t sockfd,
								in_addr_t sfs_addr, uint16_t sfs_port);

static void clid_process_commands(int32_t app_sockfd,
								  in_addr_t sfs_addr, uint16_t sfs_port);

static void clid_handle_sigterm(int signum);


static volatile int32_t not_terminating;

static uint8_t *command_buffer = NULL;

int32_t main(int argc, char *argv[])
{
	int32_t ret = 0;
	int32_t app_sockfd = -1;
	char *clid_port_str = getenv("SSTACK_CLID_PORT");
	char *clid_addr_str = getenv("SSTACK_CLID_ADDR");
	char *sfs_port_str = getenv("SSTACK_SFS_PORT");
	char *sfs_addr_str = getenv("SSTACK_SFS_ADDR");
	uint16_t clid_port, sfs_port;
	in_addr_t sfs_addr, clid_addr;

	ret = clid_validate_params(argv[0], clid_port_str, clid_addr_str,
						  sfs_port_str, sfs_addr_str, &clid_port, &clid_addr,
						  &sfs_port, &sfs_addr);
	if (ret != 0)
		return ret;

	/* Register signal handlers */
	if (signal(SIGTERM, clid_handle_sigterm) == SIG_ERR)
		return -errno;

	/* Allocate command buffer */
	if ((command_buffer = malloc(SFSCLI_MAX_BUFFER_SIZE)) == NULL)
		return -ENOMEM;

	/* O.K we have valid parameters to start */
	app_sockfd = clid_start(clid_addr, clid_port);
	printf ("App sockfd: %d\n", app_sockfd);

	if (app_sockfd < 0) {
		fprintf(stderr, "Unable to create local socket, Quitting..");
		return -errno;
	}

	not_terminating = 1;

	//daemon(0,0);
	clid_process_commands(app_sockfd, sfs_addr, sfs_port);

	free(command_buffer);


	return ret;
}


/* ========================== Private Functions ======================== */
static int32_t clid_validate_params(char *clid_name, char *clid_port_str,
									char *clid_addr_str, char *sfs_port_str,
									char *sfs_addr_str, uint16_t *clid_port,
									in_addr_t *clid_addr, uint16_t *sfs_port,
									in_addr_t *sfs_addr)
{
	char *laddr = clid_addr_str;
	char *raddr = sfs_addr_str;
	char *lport = clid_port_str;
	char *rport = sfs_port_str;
	uint32_t lerrno;

	if (clid_port_str == NULL) {
		lport = SFSCLI_DEF_CLID_PORT;
	}

	if (clid_addr_str == NULL) {
		laddr = SFSCLI_DEF_CLID_ADDR;
	}

	if (sfs_port_str == NULL) {
		rport = SFSCLI_DEF_SFS_PORT;
	}

	if (sfs_addr_str == NULL) {
		raddr = SFSCLI_DEF_SFS_ADDR;
	}

	fprintf(stdout, "Using %s:%s to bind\n", laddr, lport);
	fprintf(stdout, "Using %s:%s to connect SFS\n", raddr, rport);

	*clid_addr = inet_addr(laddr);
	*clid_port = strtol(lport, NULL, 10);
	lerrno = errno;

	*sfs_addr = inet_addr(raddr);
	*sfs_port = strtol(rport, NULL, 10);


	if (lerrno == ERANGE || errno == ERANGE ||
		*clid_addr == 0 || *sfs_addr == 0) {
		fprintf(stdout, "Invalid paramater to connect\n");
		return -EINVAL;
	}

	return 0;
}

static int32_t clid_start(in_addr_t clid_addr, uint16_t clid_port)
{
	struct sockaddr_in servaddr;
	struct in_addr inaddr;
	int32_t sockfd = -1;
	int32_t optval;
	int32_t ret = 0;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd > 0) {
		optval = 1;
		setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
				   (const void *)&optval , sizeof(int));
		bzero(&servaddr,sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		servaddr.sin_addr.s_addr = clid_addr;
		servaddr.sin_port = htons(clid_port);
		inaddr.s_addr = clid_addr;
		if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == 0) {
			fprintf(stdout, "Waiting for connections at %s:%d\n",
					inet_ntoa(inaddr), clid_port);
			listen(sockfd, 128);
			ret = sockfd;
		} else {
			fprintf(stderr, "Could not bind on  %s:%d\n",
					inet_ntoa(inaddr), clid_port);
			close(sockfd);
			ret = -errno;
		}
	} else {
		close(sockfd);
		ret = -errno;
	}

	return ret;
}

static int handle_sigalarm(int signum)
{
	if (signum)
		printf ("Timer fired\n");
}
static void clid_process_commands(int32_t app_sockfd,
								  in_addr_t sfs_addr, uint16_t sfs_port)
{
	int32_t sfs_sockfd = -1;
	ssize_t rnbytes = 0;
	ssize_t wnbytes = 0;
	int32_t ret = 0, rc = -1;
	struct sockaddr_in app_addr;
	socklen_t app_addr_len = sizeof(app_addr);
	int32_t app_rw_sockfd;
	uint8_t *buffer = command_buffer;
	struct itimerval it_val_arm;
	struct itimerval it_val_unarm;

	sfs_sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sfs_sockfd < 0) {
		fprintf(stderr, "Unable to create SFS socket, Quitting...\n");
		return;
	}

	signal(SIGPIPE, SIG_IGN);
	signal(SIGALRM, handle_sigalarm);

	/* Set an inactivity timer of 180 seconds */
	it_val_arm.it_interval.tv_sec = 0;
	it_val_arm.it_interval.tv_usec = 0;
	it_val_arm.it_value.tv_sec = INACTIVITY_INTERVAL_S;
	it_val_arm.it_value.tv_usec = INACTIVITY_INTERVAL_US;

	memset(&it_val_unarm, 0, sizeof(struct itimerval));

	while(not_terminating) {
		rnbytes = wnbytes = 0;
		memset(buffer, 0, SFSCLI_MAX_BUFFER_SIZE);
		/* Do an accept here and then proceed */
		app_rw_sockfd = accept(app_sockfd,
							   (struct sockaddr *)&app_addr, &app_addr_len);
		if (app_rw_sockfd < 0)
			continue;

		printf ("Connection received\n");
		setitimer(ITIMER_REAL, &it_val_unarm, 0);
		/* Receive commands from the CLI app */
		bzero(buffer, SFSCLI_MAX_BUFFER_SIZE);
		rc = -1;
		select_read_to_buffer(app_rw_sockfd, rc,
							  buffer, SFSCLI_MAX_BUFFER_SIZE, rnbytes);
		/* No processing here, just forward the buffer to SFS */
		printf ("Time to forward to sfs.. rnbytes : %d, rc : %d\n", rnbytes, rc);
		if (rnbytes > 0 && rc > 0) {
			printf ("Read %d bytes from the CLI APP\n");
			wnbytes = write(sfs_sockfd, buffer, rnbytes);
			printf ("wrote %d bytes to sfs\n", wnbytes);
			if (wnbytes < 0) {
				ret = clid_connect_sfs(sfs_sockfd, sfs_addr, sfs_port);
				if (ret == 0) {
					wnbytes = write(sfs_sockfd, buffer, rnbytes);
					printf ("wnbytes (2nd) %d bytes to sfs\n", wnbytes);
				}
			}
		}

		if (wnbytes > 0) {
			rnbytes = 0;
			rc = -1;
			/* Wait for a response from SFS */
			select_read_to_buffer(sfs_sockfd, rc,
								  buffer, SFSCLI_MAX_BUFFER_SIZE, rnbytes);
			if (rnbytes > 0 && rc > 0) {
				printf ("Read %d bytes from SFS\n", rnbytes);
				wnbytes = write(app_rw_sockfd, buffer, rnbytes);
				printf ("Wrote %d bytes to the APP\n", wnbytes);
			} else {
				/* TODO: Error reading from SFS, send an error to CLI app */
			}
		} else {
			/* Send an error to CLI app */
		}
		close(app_rw_sockfd);
		setitimer(ITIMER_REAL, &it_val_arm, 0);

	}

	/* cleanup and return */
	close(app_sockfd);
	close(sfs_sockfd);
	return;
}

static int32_t clid_connect_sfs(int32_t sockfd,
								in_addr_t sfs_addr,uint16_t sfs_port)
{
	struct sockaddr_in servaddr;
	int32_t ret = -1;

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = sfs_addr;
	servaddr.sin_port = htons(sfs_port);
	printf ("clid Connecting to sfs...\n");
	ret = connect(sockfd, &servaddr, sizeof(servaddr));

	return ret;
}

static void clid_handle_sigterm(int signum)
{
	if (signum == SIGTERM)
		not_terminating = 0;
}
