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


void usage(char *progname)
{
	fprintf(stderr, "Usage: %s [add|del] \"path\"  hostname\n", progname);
	fprintf(stderr, "Usage: %s [add|del] \"policy\"  hostname fname ftype uid"
			"gid is_hidden is_striped qoslevel extent_size\n", progname);
	exit(-1);
}

size_t serialize_branch(sfs_client_request_t *req, char* buf)
{
	size_t size = 0;

	if (!req)
		return size;

	if (buf) {
		unsigned int tmp;

		tmp = htonl(req->u1.req_magic);
		memcpy(buf, &tmp, sizeof(int));
		size += sizeof(int);
		tmp = htonl(req->u1.req_type);
		memcpy(buf + size, &tmp, sizeof(int));
		size += sizeof(int);
		memcpy(buf + size, req->u1.branches, PATH_MAX);
		size += PATH_MAX;
		memcpy(buf + size, req->u1.login_name, LOGIN_NAME_MAX);
		size += LOGIN_NAME_MAX;
	}

	return size;
}

size_t serialize_policy(sfs_client_request_t *req, char *buf)
{
	size_t size = 0;

	if (!req)
		return size;

	if (buf) {
		unsigned int tmp;
		uint64_t t = 0;
		uint8_t *ptr = NULL;

		tmp = htonl(req->u2.req_magic);
		memcpy(buf, &tmp, sizeof(int));
		size += sizeof(int);	
		tmp = htonl(req->u2.req_type);	
		memcpy(buf + size, &tmp, sizeof(int));
		size += sizeof(int);
		memcpy(buf + size, req->u2.req_fname, PATH_MAX);
		size += PATH_MAX;
		memcpy(buf + size, req->u2.req_ftype, TYPENAME_MAX);
		size += TYPENAME_MAX;
		tmp = htonl((int) req->u2.req_uid);
		memcpy(buf + size, &tmp, sizeof(int));
		size += sizeof(int);
		tmp = htonl((int) req->u2.req_gid);
		memcpy(buf + size, &tmp, sizeof(int));
		size += sizeof(int);
		memcpy(buf + size, &req->u2.req_is_hidden, sizeof(uint8_t));
		size += sizeof(uint8_t);
		memcpy(buf + size, &req->u2.req_is_striped, sizeof(uint8_t));
		size += sizeof(uint8_t);
		memcpy(buf + size, &req->u2.req_qoslevel, sizeof(uint8_t));
		size += sizeof(uint8_t);
		// serialize req_extent_size
		ptr = (uint8_t *) &req->u2.req_extent_size;
		t |= ((uint64_t)(*(ptr + 7)) << (7 * 8));
		t |= ((uint64_t)(*(ptr + 6)) << (6 * 8));
		t |= ((uint64_t)(*(ptr + 5)) << (5 * 8));
		t |= ((uint64_t)(*(ptr + 4)) << (4 * 8));
		t |= ((uint64_t)(*(ptr + 3)) << (3 * 8));
		t |= ((uint64_t)(*(ptr + 2)) << (2 * 8));
		t |= ((uint64_t)(*(ptr + 1)) << (1 * 8));
		t |= (uint64_t) *ptr;
		memcpy(buf + size, &t, sizeof(uint64_t));
		size += sizeof(uint64_t);
	}

	return size;
}	


int main(int argc, char *argv[])
{
	sfs_client_request_t req;
	struct addrinfo hints, *servinfo, *p;
	int rv = -1;
	int sockfd;
	char buf[sizeof(sfs_client_request_t)];


	// At least 4 arguments are needed for any supported command
	if (argc < 4) 
		usage(argv[0]);

	memset(&req, '\0', sizeof(sfs_client_request_t));
	if (strcmp(argv[2], "policy") == 0) {
		if (argc < 12)
			usage(argv[0]);

		req.u2.req_magic = SFS_MAGIC;	
		strncpy(req.u2.req_fname, argv[4], strlen(argv[4]));
		strncpy(req.u2.req_ftype, argv[5], strlen(argv[5]));
		// Following two arguments ca take '*'. So passing string as is
		strncpy((char *) &req.u2.req_uid, argv[6], strlen(argv[6]));
		strncpy((char *) &req.u2.req_gid, argv[7], strlen(argv[7]));
		req.u2.req_is_hidden = atoi(argv[8]);
		req.u2.req_is_striped = atoi(argv[9]);
		req.u2.req_qoslevel = atoi(argv[10]);
		req.u2.req_extent_size = atoll(argv[11]);
		if (strcmp(argv[1], "add") == 0)
			req.u2.req_type = ADD_POLICY;
		else if (strcmp(argv[1], "del") == 0)
			req.u2.req_type = DEL_POLICY;
	} else {
		// Only other option is "path"
		req.u1.req_magic = SFS_MAGIC;	
		strncpy(req.u1.branches, argv[2], PATH_MAX);
		strncpy(req.u1.login_name, getlogin(), LOGIN_NAME_MAX);
		if (strcmp(argv[1], "add") == 0)
			req.u1.req_type = ADD_BRANCH;
		else if (strcmp(argv[1], "del") == 0)
			req.u1.req_type = DEL_BRANCH;
	}

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(argv[3], CLI_PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return -1;
	}

	// loop through all the results and connect to the first we can
	for (p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
			perror("sfscli: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("sfscli: connect");
			continue;
		}
		break;
	}

	if ( NULL == p) {
		fprintf(stderr, "sfscli: failed to connect \n");
		close(sockfd);
		return -1;
	}

	freeaddrinfo(servinfo);

	if (req.u1.req_type == ADD_BRANCH || req.u1.req_type == DEL_BRANCH)
		serialize_branch(&req, (char *) buf);
	else if (req.u1.req_type == ADD_POLICY || req.u1.req_type == DEL_POLICY)
		serialize_policy(&req, (char *) buf);
	else
		usage(argv[0]);

	if (send(sockfd, buf, sizeof(sfs_client_request_t), 0) == -1) {
		perror("sfscli: send");
	} else
		fprintf(stderr, "Request sent\n");

	close(sockfd);

	return 0;
}
