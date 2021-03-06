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
#include <string.h>
#include <sstack_log.h>
#include <sstack_db.h>
#include <sstack_types.h>
#include <sstack_transport.h>
#include <sstack_thread_pool.h>
#define ROOT_SEP ":"
#define ADD_BRANCH 1
#define DEL_BRANCH 2
#define ADD_POLICY 3
#define DEL_POLICY 4
#define TYPENAME_MAX 256
#define SFS_MAGIC 0x11101974
#define SFSCLI_PORT "24497"
#define SFS_SERVER_PORT "24496"
#define LISTEN_QUEUE_SIZE 128
#define IPV6_ADDR_LEN 40
#define IPV4_ADDR_LEN 40
#define MOUNTPOINT_MAXPATH 256
// Form is <ipaddr>,<path>,<[r|rw]>,<weight>
#define BRANCH_MAX (IPV6_ADDR_LEN + 1 + PATH_MAX + 1 + 2 + 1 + 6)
#define SFS_MAGIC 0x11101974 // A unique number to differentiate FS
#define IPv4 1
#define IPv6 0

typedef enum {
	KEY_BRANCHES,
	KEY_LOG_FILE_DIR,
	KEY_HELP,
	KEY_VERSION,
	KEY_LOG_LEVEL,
	KEY_DEBUG,
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
extern db_t *db;
extern sstack_client_handle_t sfs_handle;
extern sstack_thread_pool_t *sfs_thread_pool;
extern void * cli_process_thread(void *);
char * get_local_ip(char *, int , log_ctx_t *);

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
	unsigned long num_clients; // Number of sfsds talking to this sfs
	sfsd_info_t *info;
	// TBD
} sfs_metadata_t;


#endif // __SFS_H_
