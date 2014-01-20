/*************************************************************************
 *
 * SEAMLESSSTACK CONFIDENTIAL
 * __________________________
 *
 *  [2012] - [2014]  SeamlessStack Inc
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
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sstack_types.h>
#include <sfscli.h>
#include <sfscli_clid.h>

static int32_t not_terminating = 1;
static int32_t connection_dropped = 1;

static void handle_cli_requests(int32_t sockfd);

static void *
init_cli_thread(void *arg)
{
	int32_t sockfd;
	int32_t optval;
	struct sockaddr_in servaddr;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd > 0) {
		optval = 1;
		setsockopt(sockfd,  SOL_SOCKET, SO_REUSEADDR,
				   (const void *)&optval, sizeof(int32_t));
		bzero(&servaddr, sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		servaddr.sin_addr.s_addr = INADDR_ANY;
		servaddr.sin_port = htons(atoi(SFSCLI_DEF_SFS_PORT));

		if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == 0) {
			printf ("Bind done, waiting for incoming connections %s\n",
							SFSCLI_DEF_SFS_PORT);
			listen(sockfd, 5);
			handle_cli_requests(sockfd);
		}
	}
	return NULL;
}

ssize_t
get_sfsd_command_response(uint8_t *buffer, size_t buf_len, uint8_t **resp_buf)
{
	/* All the validation checks are done in get_command_response
	   No validation here
	*/
	struct sfscli_cli_cmd *cmd;

	if (sfscli_deserialize_sfsd(buffer, buf_len, &cmd) != 0)
		return 0;

	/* Now we have a valid cli_cmd structure in place, lets
	   process it */

	switch(cmd->input.sfsd_cmd) {
	case SFSD_ADD_CMD:
		break;
	default:
		printf ("Not implemented\n");
	}

}

ssize_t
get_command_response(uint8_t *buffer, size_t buf_len, uint8_t **resp_buf)
{
	uint32_t magic = 0;
	sfscli_cmd_types_t cmd;
	uint8_t *p = buffer;
	size_t resp_len;

	if (p == NULL) {
		printf ("Invalid input buffer \n");
		return -ENOMEM;
	}

	/* Check for the magic */
	sfscli_deser_uint(magic, buffer, 4);

	if (magic != SFSCLI_MAGIC) {
		printf ("Magic not found\n");
		return -EINVAL;
	}
	p+=4;

	/* Peek into the command structure to
	 * see what command is it */
	sfscli_deser_nfield(cmd, p);

	switch (cmd) {
		case SFSCLI_SFSD_CMD:
			resp_len = get_sfsd_command_response(buffer, buf_len, resp_buf);
			break;
		default:
			printf ("Not implemented\n");
			resp_len = 0;
			break;
	}

	return resp_len;
}

void
handle_cli_requests(int32_t sockfd)
{
	int32_t conn_sockfd = -1;
	struct sockaddr_in client_addr;
	socklen_t client_addr_len;
	uint8_t *cmd_buffer = NULL;
	uint8_t *resp_buffer = NULL;
	size_t rnbytes, resp_size, wnbytes;
	int32_t rc = -1;

	cmd_buffer = malloc(SFSCLI_MAX_BUFFER_SIZE);

	if (!cmd_buffer) {
		/* print a error message here */
		return;
	}

	while(not_terminating) {
		rnbytes = wnbytes = 0;
		memset(cmd_buffer, 0, SFSCLI_MAX_BUFFER_SIZE);
		printf ("Waiting for an incoming connection\n");
		/* Accept connections and process */
		if (connection_dropped == 1) {
			conn_sockfd = accept(sockfd,
								 (struct sockaddr *)&client_addr,
								 &client_addr_len);
			if (conn_sockfd < 0) {
				printf ("coulnot accept the incoming connection\n");
				continue;
			}
			connection_dropped = 0;
			printf ("Connection received from clid\n");
		}

		rc = -1;
		select_read_to_buffer(conn_sockfd, rc, cmd_buffer,
							  SFSCLI_MAX_BUFFER_SIZE, rnbytes);
		if (rnbytes > 0 && rc > 0) {
			resp_size = get_command_response(cmd_buffer, rnbytes, &resp_buffer);
			if (resp_buffer && (resp_size > 0)) {
				wnbytes = write(conn_sockfd, resp_buffer, resp_size);
				if (wnbytes < resp_size)
					printf ("Less no of response sent\n");
				else
					printf ("Wrote %ld bytes to clid\n", wnbytes);
			}
		} else {
			printf ("Error reading command\n");
		}
	}
}

int
main(void)
{
	init_cli_thread(NULL);
}
