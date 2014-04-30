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
#include <stdlib.h>
#include <unistd.h>
#include <sys/errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <sstack_jobs.h>
#include <sstack_transport.h>
#include <sstack_log.h>

#define PORT "24496"
#define CONNECT_PORT 24496

/*
 * sigchld_handler - Zombie reaper
 */

static void
sigchld_handler(int signal)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

/*
 * tcp_client_init - Init part of client(sfsd)
 *
 * transport - transport structure for the client (sfsd)
 *
 * Returns client handle i.e. socket descriptor got after connet
 */

sstack_client_handle_t
tcp_client_init(sstack_transport_t *transport)
{
	struct sigaction sa;
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

	sa.sa_handler = sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		sfs_log(transport->ctx, SFS_ERR, "%s: sigaction failed with"
			"error %d\n", __FUNCTION__, errno);
		close(sockfd);

		return  -1;
	}

	return sockfd;
}


/*
 * tcp_tx - Send payload
 *
 * handle - client handle i.e. socket fd
 * payload_len - Length of the payload. sfs(d) uses packed size of
 * sstack_payload_t which is <= sizeof(sstack_payload_t).
 * payload - Real payload to be transmitted. sfs(d) use sstack_payload_t
 *
 * Returns number of bytes transmitted on success and -2 on failure.
 */

int
tcp_tx(sstack_client_handle_t handle, size_t payload_len, void *payload,
		log_ctx_t *ctx)
{
	sfs_log(ctx, SFS_DEBUG, "%s: handle %d payload_len %d payload 0x%x \n",
		__FUNCTION__, handle, payload_len, payload);
	return send((int) handle, payload, payload_len, 0);
}

/*
 * tcp_rx - Receive payload
 *
 * handle - client handle i.e. socket fd
 * payload_len - Length of the payload. sfs(d) uses packed size of
 * sstack_payload_t which is <= sizeof(sstack_payload_t).
 * payload - Real payload to be transmitted. sfs(d) use sstack_payload_t
 *
 * Reads sstack_payload_hdr_t to determine real payload size. Returns
 * sizeof(sstack_payload_hdr_t) + payload length on success.
 *
 * Returns number of bytes received on success and a negative number
 * indicating errno on failure.
 */

int
tcp_rx(sstack_client_handle_t handle, size_t payload_len, void *payload,
		log_ctx_t * ctx)
{
	sstack_payload_hdr_t hdr;
	int ret = -1;
	int count = 0;
	int bytes_read = 0;
	int bytes_remaining = 0;

	sfs_log(ctx, SFS_DEBUG, "%s: handle %d payload_len %d payload 0x%x \n",
			__FUNCTION__, handle, payload_len, payload);

	if (handle < 0 || payload_len <= 0 || NULL == payload) {
		// Invalid parameters
		return -EINVAL;
	}

	ret = recv((int) handle, (void *) payload, sizeof(sstack_payload_t),
			0);
	if (ret == -1) {
		sfs_log(ctx, SFS_ERR, "%s: recv returned error %d\n", __FUNCTION__,
				errno);
		return -errno;
	}

	sfs_log(ctx, SFS_DEBUG, "%s: Returning %d\n", __FUNCTION__, ret);

	return ret;

#ifdef TRANSPORT_OPTIMIZATION
	ret = recv((int) handle, (void *) &hdr,
			sizeof(sstack_payload_hdr_t), 0);
	if (ret == -1) {
		// Receive failed
		return -errno;
	}
	memcpy((void *) payload, (void *) &hdr, sizeof(sstack_payload_hdr_t));
	bytes_read += sizeof(sstack_payload_hdr_t);
	bytes_remaining =  hdr.payload_len;
	// Go ahead and read rest of the payload
	ret = recv((int) handle, (char *) payload +
			sizeof(sstack_payload_hdr_t), hdr.payload_len, 0);
	if (ret == -1) {
		// Receive failed
		return -errno;
	} else if (ret == hdr.payload_len) {
		// recv succeeded.
		return (sizeof(sstack_payload_hdr_t) + hdr.payload_len);
	} else {
		bytes_remaining -= ret;
		bytes_read += ret;
		while (bytes_remaining) {
			if (count == MAX_RECV_RETRIES) {
				// Failed to recv payload
				return -errno;
			}
			ret = recv((int) handle, (char *) payload + bytes_read,
				bytes_remaining, 0);
			if (ret == -1) {
				// Failure
				return -errno;
			} else  if (ret == bytes_remaining) {
				// Recv succeeded
				return (sizeof(sstack_payload_hdr_t) +
						hdr.payload_len);
			} else {
				count ++;
				bytes_read  += ret;
				bytes_remaining -= ret;
			}
		}
	}

	return count;
#endif

}

/*
 * tcp_select - helper function to wait for connections
 *
 * handle - socket decriptor
 * flags - block mask
 *
 */

int
tcp_select(sstack_client_handle_t handle, uint32_t block_flags)
{
	fd_set readfds, writefds;
	struct timeval timeout;
	int ret;

	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	FD_SET(handle, &readfds);
	FD_SET(handle, &writefds);

	timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	/* Call the select now. */
	ret = select(handle + 1, &readfds, &writefds, NULL, &timeout);

	ASSERT((ret >= 0), "Select failed",1, 0, 0);
	if (ret != 0) {
		if (FD_ISSET(handle, &readfds) &&
				(block_flags & READ_BLOCK_MASK)) {
			return READ_NO_BLOCK;
		} else if (FD_ISSET(handle, &writefds) &&
				(block_flags & WRITE_BLOCK_MASK)) {
			return WRITE_NO_BLOCK;
		}
	}

	return IGNORE_NO_BLOCK;
}



/*
 * tcp_server_setup - Server (sfs) side socket setup
 *
 * transport - transport structure of the server(sfs)
 *
 * Creates the socket and binds it to the IP specified in the transport
 * and starts listening on socket.
 *
 * Returns 0 on success and -1 on failure with errno set to proper value.
 */

sstack_client_handle_t
tcp_server_setup(sstack_transport_t *transport)
{
	struct sigaction sa;
	struct sockaddr_in addr;
	int sockfd = -1;
	int yes = 1;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		sfs_log(transport->ctx, SFS_ERR,
				"%s: Socket creation failed with"
				"error %d\n", __FUNCTION__, errno);
		return -1;
	}
	sfs_log(transport->ctx, SFS_DEBUG, "%s: Socket fd = %d \n",
					__FUNCTION__, sockfd);

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
	sfs_log(transport->ctx, SFS_DEBUG, "%s: bind succeeded \n",
					__FUNCTION__);

	if (listen(sockfd, SSTACK_BACKLOG) == -1) {
		sfs_log(transport->ctx, SFS_ERR, "%s: listen failed with"
			"error %d\n", __FUNCTION__, errno);
		close(sockfd);

		return -1;
	}
	sfs_log(transport->ctx, SFS_DEBUG, "%s: listen succeeded \n",
					__FUNCTION__);

	sa.sa_handler = sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		sfs_log(transport->ctx, SFS_ERR, "%s: sigaction failed with"
			"error %d\n", __FUNCTION__, errno);
		close(sockfd);

		return  -1;
	}

	sfs_log(transport->ctx, SFS_INFO, "%s: Succeeded \n", __FUNCTION__);

	return  (sstack_handle_t) sockfd;
}

/*
 * get_tcp_transport - Helper function to populate transport structure
 *
 * addr - IPv4 address for the transport
 *
 * Returns NULL upon failure and transport structure upon success.
 */

sstack_transport_t *get_tcp_transport(char *addr, log_ctx_t *ctx)
{
	sstack_transport_t *transport = NULL;

	transport = alloc_transport();

	ASSERT((transport != NULL),
			"Failed to allocate tcp transport", 1, 1, 0);

	transport->transport_type = TCPIP;
	transport->transport_hdr.tcp.ipv4 = 1;
	strcpy(transport->transport_hdr.tcp.ipv4_addr, addr);
	transport->transport_hdr.tcp.port = htons(CONNECT_PORT);
	transport->ctx = ctx;

	transport->transport_ops.client_init = tcp_client_init;
	transport->transport_ops.tx = tcp_tx;
	transport->transport_ops.rx = tcp_rx;
	transport->transport_ops.select = tcp_select;
	transport->transport_ops.server_setup = tcp_server_setup;

	return transport;

}
