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
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sstack_log.h>
#define DEBUG 1

extern log_ctx_t *sfs_ctx;

static char *
get_ip_str(const struct sockaddr *sa, char *s, unsigned int maxlen)
{
	switch(sa->sa_family) {
		case AF_INET:
			inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr),s,
									maxlen);
			break;

		case AF_INET6:
			inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr), s,
								maxlen);
			break;

		default:
			strncpy(s, "Unknown AF", maxlen);
			return NULL;
	}
	return s;
}

/*
 * sfs_traceroute - Find number of hops between sfs and given sfsd's address
 *
 * dest_hostname - hostname/IPaddr of the sfsd host . Should be non-NULL.
 * num_hops - Output parameter that returns number of hops between sfs & sfsd
 *
 * Returns 0 on success and -1 on failure.
 */

int
sfs_traceroute(char* dest_hostname, int *num_hops)
{
	struct addrinfo*        dest_addrinfo_collection = NULL;
	struct addrinfo*        dest_addrinfo_item = NULL;
	struct sockaddr*        dest_addr = NULL;
	struct sockaddr_in      nexthop_addr_in;
	struct timeval          tv;
	int                     recv_socket;
	int                     send_socket;
	char                    buf[512];
	char                    nexthop_hostname[NI_MAXHOST];
	char                    dest_addr_str[INET_ADDRSTRLEN];
	char                    nexthop_addr_str[INET_ADDRSTRLEN];
	unsigned int            nexthop_addr_in_len;
	unsigned short          iter;
	socklen_t               ttl = 1;
	long                    error = -1;
	socklen_t               maxhops = 64;

	// Parameter validation
	if (NULL == dest_addr || NULL == num_hops) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameter specified \n",
						__FUNCTION__);

		errno = EINVAL;
		return -1;
	}

	/* resolve the domain name into a list of addresses */
	error = getaddrinfo(dest_hostname, NULL, NULL, &dest_addrinfo_collection);
	if (error != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Error in getaddrinfo: %s\n",
						__FUNCTION__, gai_strerror((int)error));
		return -1;
	}

	/* loop over returned results */
	for (dest_addrinfo_item = dest_addrinfo_collection, iter=1;
			dest_addrinfo_item != NULL;
			dest_addrinfo_item = dest_addrinfo_item->ai_next, iter++){
		if(iter==1) {
			dest_addr = dest_addrinfo_item->ai_addr;
			get_ip_str(dest_addr, dest_addr_str, INET_ADDRSTRLEN);
		} else {
			// Destination URL has multiple addresses.
			// Use the first one
			break;
		}
	}

	tv.tv_sec = 5;
	tv.tv_usec = 0;
	while (1) {
		fd_set fds;
		int error = -1;
		struct sockaddr_in *dest_addr_in = NULL;

		FD_ZERO(&fds);
		recv_socket = socket(PF_INET, SOCK_RAW, IPPROTO_ICMP);
		if (recv_socket == -1) {
			sfs_log(sfs_ctx, SFS_ERR, "%s: Cannot create receive socket. "
							"Error = %d \n", __FUNCTION__, errno);
			freeaddrinfo(dest_addrinfo_collection);

			return -1;
		}
		FD_SET(recv_socket, &fds);
		send_socket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (send_socket == -1) {
			sfs_log(sfs_ctx, SFS_ERR, "%s: Cannot create send socket."
							"Error = %d \n", __FUNCTION__, errno);
			freeaddrinfo(dest_addrinfo_collection);

			return -1;
		}
		error = setsockopt(send_socket, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));
		if (error != 0) {
			sfs_log(sfs_ctx, SFS_ERR, "%s: Error setting socket options. "
							"Error = %d \n", __FUNCTION__, errno);
			freeaddrinfo(dest_addrinfo_collection);

			return -1;
		}
		dest_addr_in = (struct sockaddr_in*) dest_addr;
		dest_addr_in->sin_family = PF_INET;
		dest_addr_in->sin_port = htons(33434);
		error = sendto(send_socket, &buf, sizeof(buf),
					0, (struct sockaddr *)dest_addr_in, sizeof(*dest_addr_in));
		if (error == -1) {
			sfs_log(sfs_ctx, SFS_ERR, "%s: Error sending data to destination."
							" Error = %d\n", __FUNCTION__, errno);
			freeaddrinfo(dest_addrinfo_collection);

			return -1;
		}

		error = select(recv_socket+1, &fds, NULL, NULL, &tv);
		if (error == -1) {
			sfs_log(sfs_ctx, SFS_ERR, "%s: Select error. Error = %d \n",
								__FUNCTION__, errno);
			freeaddrinfo(dest_addrinfo_collection);

			return -1;
		} else if (error == 0) {
			fflush(stdin);
		} else {
			if (FD_ISSET(recv_socket, &fds)) {
				nexthop_addr_in_len = sizeof(nexthop_addr_in);
				recvfrom(recv_socket, buf, sizeof(buf),
							0, (struct sockaddr *)&nexthop_addr_in,
							&nexthop_addr_in_len);
				get_ip_str((struct sockaddr *)&nexthop_addr_in,
								nexthop_addr_str, INET_ADDRSTRLEN);
				error = getnameinfo((struct sockaddr *)&nexthop_addr_in,
									nexthop_addr_in_len,
									nexthop_hostname, sizeof(nexthop_hostname),
									NULL, 0, NI_NAMEREQD);
				if (error != 0) {
					error = getnameinfo((struct sockaddr *)&nexthop_addr_in,
										nexthop_addr_in_len,
									nexthop_hostname, sizeof(nexthop_hostname),
										NULL, 0, NI_NUMERICHOST);
					if (error != 0) {
						sfs_log(sfs_ctx, SFS_ERR, "%s: Error in getnameinfo: "
								"%s \n", __FUNCTION__, gai_strerror(error));
									freeaddrinfo(dest_addrinfo_collection);

						return -1;
					}
				}
			}
		}

		if (ttl == maxhops) {
			sfs_log(sfs_ctx, SFS_ERR, "%s: Max hops reached. Unable to find "
							"the destination host \n", __FUNCTION__);
			freeaddrinfo(dest_addrinfo_collection);
			close(recv_socket);
			close(send_socket);

			return -1;
		}

		close(recv_socket);
		close(send_socket);
		if (!strcmp(dest_addr_str, nexthop_addr_str)) {
			sfs_log(sfs_ctx, SFS_INFO, "%s: Destination reached in %d "
						"hops \n", __FUNCTION__, ttl);
			break;
		}

		ttl++;
	}

	freeaddrinfo(dest_addrinfo_collection);
	*num_hops = ttl;

	return EXIT_SUCCESS;
}

#ifdef TRACEROUTE_TEST

int
main(int argc, char* argv[]){

	int hops = 0;
	char* ipaddr = NULL;
	int ret = -1;

	if(argc!=2){
		fprintf(stderr, "\nusage: traceroute host");
		return EXIT_FAILURE;
	}

	ipaddr = argv[1];
	ret  = traceroute(ipaddr, &hops);

	printf("%s: Number of hops to %s is %d \n", argv[0], ipaddr, hops);

	return ret;
}

#endif // TRACEROUTE_TEST
