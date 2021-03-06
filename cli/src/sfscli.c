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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/errno.h>
#include <string.h>
#include <netdb.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/errno.h>
#include <sfs.h>
#include <sfscli.h>
#include <sfscli_clid.h>

/* ====================== FUNCTION DECLARATIONS ======================= */
static int32_t sfscli_connect_clid(in_addr_t clid_addr, uint16_t clid_port);
static int32_t process_args(int32_t args, char *argv[], int32_t sockfd);

/* ====================== FUNCTION DEFINITIONS ======================== */
void
usage(char *progname)
{
	fprintf(stderr, "Usage %s\n", progname);
}

int
main(int argc, char *argv[])
{
	char *clid_addr_str = getenv("SSTACK_CLID_ADDR");
	char *clid_port_str = getenv("SSTACK_CLID_PORT");
	in_addr_t clid_addr;
	uint16_t clid_port = 0;
	int32_t clid_sockfd;
	int32_t ret;

	if (argc < 2) {
		usage(argv[0]);
		return -1;
	}

	if (clid_addr_str == NULL) {
		fprintf (stdout,
				 "Using SSTACK_CLID_ADDR as %s\n", SFSCLI_DEF_CLID_ADDR);
		clid_addr_str = SFSCLI_DEF_CLID_ADDR;
	}

	if (clid_port_str == NULL) {
		fprintf(stdout,
				"Using SSTACK_CLID_PORT as %s\n", SFSCLI_DEF_CLID_PORT);
		clid_port_str = SFSCLI_DEF_CLID_PORT;
	}

	clid_addr = inet_addr(clid_addr_str);
	clid_port = strtol(clid_port_str, NULL, 0);

	if (clid_addr == 0 || errno == ERANGE) {
		fprintf (stdout, "Invalid paramters to connect CLID\n");
		return -errno;
	}

	if ((clid_sockfd = sfscli_connect_clid(clid_addr, clid_port)) < 0) {
		fprintf (stdout, "Could not connect to CLID, error %d", clid_sockfd);
		return clid_sockfd;
	}

	/* Now that we are connected we can process inputs from command line */
	ret = process_args(argc, argv, clid_sockfd);

	return ret;
}


static int32_t
sfscli_connect_clid(in_addr_t clid_addr, uint16_t clid_port)
{
	int32_t sockfd;
	struct sockaddr_in clid_addr_in;
	struct in_addr inaddr;
	pid_t clid_pid;

	inaddr.s_addr = clid_addr;
	bzero(&clid_addr_in, sizeof(clid_addr_in));

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0)
		return -errno;

	clid_addr_in.sin_addr.s_addr = clid_addr;
	clid_addr_in.sin_family = AF_INET;
	clid_addr_in.sin_port = htons(clid_port);

	if (connect(sockfd, (struct sockaddr *)&clid_addr_in,
							sizeof(clid_addr_in)) < 0) {
		if (errno == ECONNREFUSED) {
			/* Probably the CLID daemon is not running.. exec it */
			printf ("initial connection refused\n");
			if ((clid_pid = fork()) == 0) {
				int ret;
				printf ("forking execing..\n");
				ret = execlp("sfsclid", "sfsclid", NULL);
				printf ("could not exec\n");
				if (ret < 0)
					kill(0, SIGKILL);
			}
			/* Wait for the daemon to be up */
			sleep(1);
			/* Try reconnecting again */
			if (connect(sockfd, (struct sockaddr *)&clid_addr_in,
					sizeof(clid_addr_in)) < 0) {
				/* Something else has gone wrong.. Cleanup.. */
				kill(clid_pid, SIGTERM);
				close(sockfd);
				sockfd = -errno;
				close(sockfd);
				fprintf (stderr, "Could not connect to CLID at %s:%d\n",
						 inet_ntoa(inaddr), clid_port);
			}
		}
	}

	inaddr.s_addr = clid_addr;
	fprintf (stdout, "Connected to CLID at %s:%d, sockfd = %d\n",
			 inet_ntoa(inaddr), clid_port, sockfd);
	return sockfd;
}

int32_t
process_args(int32_t argc, char *argv[], int32_t sockfd)
{
	char *command_str = NULL;
	struct sfscli_cli_cmd *cli_cmd = NULL, *an_cmd = NULL;
	uint8_t *buffer = NULL;
	size_t buf_len = 0;

	if (argc == 1) {
		usage(argv[0]);
		return -EINVAL;
	}

	command_str = basename(argv[0]);
	if (!strcmp(argv[1], "storage") || !strcmp(command_str, "storage")) {
		cli_cmd = parse_fill_storage_input(argc, argv);
		if (cli_cmd == NULL)
			return -1;
		else {
			buf_len = sfscli_serialize_storage(cli_cmd, &buffer);
			printf ("Now deserialize--\n");
			sfscli_deserialize_storage(buffer, buf_len, &an_cmd);
		}
	} else if (!strcmp(argv[1], "policy") || !strcmp(command_str, "policy")) {
		cli_cmd = parse_fill_policy_input(argc, argv);
		if (cli_cmd == NULL) {
			return -1;
		} else {
			buf_len = sfscli_serialize_policy(cli_cmd, &buffer);
			sfscli_deserialize_policy(buffer, buf_len, &an_cmd);
		}
	} else if (!strcmp(argv[1], "sfsd") || !strcmp(command_str, "sfsd")) {
		cli_cmd = parse_fill_sfsd_input(argc, argv);
		if (cli_cmd == NULL)
			return -1;
		else {
			buf_len = sfscli_serialize_sfsd(cli_cmd, &buffer);
			printf ("Now deserialize sfsd--\n");
			sfscli_deserialize_sfsd(buffer, buf_len, &an_cmd);
		}
	} else if (!strcmp(argv[1], "license") ||
					!strcmp(command_str, "license")) {
		cli_cmd = parse_fill_license_input(argc, argv);
		if (cli_cmd == NULL)
			return -1;
		else {
			buf_len = sfscli_serialize_license(cli_cmd, &buffer);
			printf ("Now deserialize sfsd--\n");
			sfscli_deserialize_license(buffer, buf_len, &an_cmd);
		}
		printf ("license command\n");
	} else {
		printf ("invalid command\n");
	}

	if (buf_len) {
		for(int i = 0; i < buf_len ; ++i)
			printf ("%#x ", buffer[i]);
		printf ("\n");
		/* Send message to CLID */
		int ret  = write(sockfd, buffer, buf_len);
		printf ("Wrote %d bytes\n", ret);
		/* Wait for response */

	}

	return 0;
}
