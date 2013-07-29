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
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <sstack_transport.h>
#include <sstack_log.h>

#define PORT "24496"

sstack_client_handle_t
tcp_client_init(sstack_transport_t *transport)
{
	struct sockaddr_in addr = { 0 };
	int sockfd  = -1;

	// Only IPv4 implemented.
	// Use ipv4 field in the sstack_transport_t.tcp to determine 
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		sfs_log(transport->ctx, SFS_ERR, "%s: Socket creation failed with"
			"error %d\n", __FUNCTION__, errno); 
		return -1;
	}

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr =
		inet_addr(transport->transport_hdr.tcp.ipv4_addr);
	addr.sin_port = transport->transport_hdr.tcp.port;

	if (connect(sockfd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		sfs_log(transport->ctx, SFS_ERR, "%s: Connect failed with"
			"error %d\n", __FUNCTION__, errno); 
		return -1;
	}

	return sockfd;
}


int
tcp_tx(sstack_client_handle_t handle, size_t payload_len, void *payload)
{
	return send((int) handle, payload, payload_len, 0);
}

int
tcp_rx(sstack_client_handle_t handle, size_t payload_len, void *payload)
{
	return recv((int) handle, payload, payload_len, 0);
}


static void
sigchld_handler(int signal)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}


sstack_client_handle_t
tcp_server_setup(sstack_transport_t *transport)
{
	struct sigaction sa;
	struct sockaddr_in addr;
	int sockfd = -1;
	int yes = 1;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		sfs_log(transport->ctx, SFS_ERR, "%s: Socket creation failed with"
			"error %d\n", __FUNCTION__, errno); 
		return -1;
	}

	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
			sizeof(int)) == -1) {
		sfs_log(transport->ctx, SFS_ERR, "%s: setsockopt failed with"
			"error %d\n", __FUNCTION__, errno); 
		return -1;
	}

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr =
		inet_addr(transport->transport_hdr.tcp.ipv4_addr);
	addr.sin_port = transport->transport_hdr.tcp.port;

	if (bind(sockfd, (const struct sockaddr *) &addr, sizeof(addr)) == -1) {
		sfs_log(transport->ctx, SFS_ERR, "%s: bind failed with"
			"error %d\n", __FUNCTION__, errno); 
		close(sockfd);
		return -1;
	}

	if (listen(sockfd, SSTACK_BACKLOG) == -1) {
		sfs_log(transport->ctx, SFS_ERR, "%s: listen failed with"
			"error %d\n", __FUNCTION__, errno); 
		close(sockfd);

		return -1;
	}

	sa.sa_handler = sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		sfs_log(transport->ctx, SFS_ERR, "%s: sigaction failed with"
			"error %d\n", __FUNCTION__, errno); 
		close(sockfd);

		return  -1;
	}

	return  (sstack_handle_t) sockfd;
}
