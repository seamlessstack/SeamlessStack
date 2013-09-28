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

#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sstack_log.h>
#include <sstack_db.h>
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
#define CLI_PORT "24496"
#define LISTEN_QUEUE_SIZE 128
#define IPV6_ADDR_LEN 40
// Form is <ipaddr>,<path>,<[r|rw]>,<weight>
#define BRANCH_MAX (IPV6_ADDR_LEN + 1 + PATH_MAX + 1 + 2 + 1 + 6)

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

typedef struct sfs_clinet_request_hdr {
	uint32_t magic;
	int type;
} sfs_clinet_request_hdr_t;

typedef struct sfs_client_request {
	sfs_clinet_request_hdr_t hdr;
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


#endif // __SFS_H_
