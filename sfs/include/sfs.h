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

#ifndef __SFS_H_
#define __SFS_H_

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sstack_log.h>
#include <sstack_db.h>
#include <sstack_types.h>
#include <sstack_transport.h>
#define ROOT_SEP ":"
#define MINIMUM_WEIGHT 0
#define DEFAULT_WEIGHT 5
#define MAXIMUM_WEIGHT 65536
#define ADD_BRANCH 1
#define DEL_BRANCH 2
#define ADD_POLICY 3
#define DEL_POLICY 4
#define TYPENAME_MAX 256
#define SFS_MAGIC 0x11101974
#define CLI_PORT "24497"
#define SFS_SERVER_PORT 24496
#define LISTEN_QUEUE_SIZE 128
#define IPV6_ADDR_LEN 40
// Form is <ipaddr>,<path>,<[r|rw]>,<weight>
#define BRANCH_MAX (IPV6_ADDR_LEN + 1 + PATH_MAX + 1 + 2 + 1 + 6)
#define SFS_MAGIC 0x11101974 // A unique number to differentiate FS

typedef enum {
	KEY_BRANCHES,
	KEY_LOG_FILE_DIR,
	KEY_HELP,
	KEY_VERSION,
	KEY_LOG_LEVEL
} key_type_t;

typedef struct sfs_chunk_entry {
	char ipaddr[IPV6_ADDR_LEN];
	char *chunk_path;
	int chunk_pathlen;
	unsigned char rw;
	uint32_t weight; // weight associated with the branch
	uint8_t	inuse;
} sfs_chunk_entry_t;

typedef struct sfs_client_request_hdr {
	uint32_t magic;
	int type;
} sfs_client_request_hdr_t;

typedef struct sfs_client_request {
	sfs_client_request_hdr_t hdr;
	union {
		struct branch_struct_cli {
			char branches[PATH_MAX + IPV6_ADDR_LEN + 1];
			char login_name[LOGIN_NAME_MAX];
		} u1;
		struct policy_struct_cli {
			char fname[PATH_MAX];
			char ftype[TYPENAME_MAX];
			uid_t uid;
			gid_t gid;
			int hidden;
			int qoslevel;
		} u2;
	};
} sfs_client_request_t;

static inline void *
get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in *)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

static inline char *
rep(char *src, char slash)
{
	char *p = NULL;
	char prev_char;
	int i = 0;

	p = src;
	prev_char = *p;

	while (*p != '\0') {
		p++;
		if (prev_char == slash && *p == slash)
			continue;
		i++;
		*(src+i) = *p;
		prev_char = *p;
	}

	return src;
}

extern uint32_t sstack_checksum(log_ctx_t *, const char *);
extern log_ctx_t *sfs_ctx;
extern db_t *db;
extern sstack_client_handle_t sfs_handle;

/*
 * A simple structure that holds infomation on how to contact sfsd
 */

typedef struct sfsd_info {
	sstack_transport_t *transport;
	sstack_client_handle_t handle;
} sfsd_info_t;

/*
 * File system metadata. Required for commands like df that issue statvfs(2).
 */

typedef struct sfs_metadata {
	unsigned long num_clients; // Number of sfsds taling to this sfs
	sfsd_info_t *info;
	// TBD
} sfs_metadata_t;


static inline char *
get_local_ip(char *interface)
{
	int fd;
	struct ifreq ifr;

	fd = socket(AF_INET, SOCK_DGRAM, 0);

	// IPv4 IP address 
	// For IPv6 address, use AF_INET6
	ifr.ifr_addr.sa_family = AF_INET;

	strncpy(ifr.ifr_name, interface, IFNAMSIZ - 1);

	ioctl(fd, SIOCGIFADDR, &ifr);

	close(fd);

	return inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
}

#endif // __SFS_H_
