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
	fprintf(stderr, "\t\t where path is in form ipv[46]addr,path,r/w,"
		"storage_weight\n");
	fprintf(stderr, "Usage: %s [add|del] \"policy\"  hostname fname ftype uid"
			"gid is_hidden qoslevel\n", progname);
	exit(-1);
}

size_t serialize_branch(sfs_client_request_t *req, char* buf)
{
	size_t size = 0;

	if (!req)
		return size;

	if (buf) {
		unsigned int tmp;

		tmp = htonl(req->hdr.magic);
		memcpy(buf, &tmp, sizeof(int));
		size += sizeof(int);
		tmp = htonl(req->hdr.type);
		memcpy(buf + size, &tmp, sizeof(int));
		size += sizeof(int);
		memcpy(buf + size, req->u1.branches, BRANCH_MAX);
		size += BRANCH_MAX;
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

		tmp = htonl(req->hdr.magic);
		memcpy(buf, &tmp, sizeof(int));
		size += sizeof(int);	
		tmp = htonl(req->hdr.type);	
		memcpy(buf + size, &tmp, sizeof(int));
		size += sizeof(int);
		memcpy(buf + size, req->u2.fname, PATH_MAX);
		size += PATH_MAX;
		memcpy(buf + size, req->u2.ftype, TYPENAME_MAX);
		size += TYPENAME_MAX;
		tmp = htonl((int) req->u2.uid);
		memcpy(buf + size, &tmp, sizeof(int));
		size += sizeof(int);
		tmp = htonl((int) req->u2.gid);
		memcpy(buf + size, &tmp, sizeof(int));
		size += sizeof(int);
		tmp = htonl((int) req->u2.hidden);
		memcpy(buf + size, &tmp, sizeof(int));
		size += sizeof(int);
		tmp = htonl((int) req->u2.qoslevel);
		memcpy(buf + size, &tmp, sizeof(int));
		size += sizeof(int);
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
	req.hdr.magic = SFS_MAGIC;	

	if (strcmp(argv[2], "policy") == 0) {
		if (argc < 12)
			usage(argv[0]);

		strncpy(req.u2.fname, argv[4], strlen(argv[4]));
		strncpy(req.u2.ftype, argv[5], strlen(argv[5]));
		// Following two arguments ca take '*'. So passing string as is
		strncpy((char *) &req.u2.uid, argv[6], strlen(argv[6]));
		strncpy((char *) &req.u2.gid, argv[7], strlen(argv[7]));
		req.u2.hidden = atoi(argv[8]);
		req.u2.qoslevel = atoi(argv[9]);
		if (strcmp(argv[1], "add") == 0)
			req.hdr.type = ADD_POLICY;
		else if (strcmp(argv[1], "del") == 0)
			req.hdr.type = DEL_POLICY;
	} else {
		// Only other option is "path"
		strncpy(req.u1.branches, argv[2], BRANCH_MAX);
		strncpy(req.u1.login_name, getlogin(), LOGIN_NAME_MAX);
		if (strcmp(argv[1], "add") == 0)
			req.hdr.type = ADD_BRANCH;
		else if (strcmp(argv[1], "del") == 0)
			req.hdr.type = DEL_BRANCH;
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

	if (req.hdr.type == ADD_BRANCH || req.hdr.type == DEL_BRANCH)
		serialize_branch(&req, (char *) buf);
	else if (req.hdr.type == ADD_POLICY || req.hdr.type == DEL_POLICY)
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