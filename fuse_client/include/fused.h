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

#ifndef __FUSED_H_
#define __FUSED_H_

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
#define FUSED_MAGIC 0x11101974
#define FUSEDCLI_PORT "24497"
#define FUSED_SERVER_PORT "24496"
#define LISTEN_QUEUE_SIZE 128
#define IPV6_ADDR_LEN 40
#define IPV4_ADDR_LEN 40
#define MOUNTPOINT_MAXPATH 256
// Form is <ipaddr>,<path>,<[r|rw]>,<weight>
#define BRANCH_MAX (IPV6_ADDR_LEN + 1 + PATH_MAX + 1 + 2 + 1 + 6)
#define FUSED_MAGIC 0x11101974 // A unique number to differentiate FS
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

char * get_local_ip(char *, int , log_ctx_t *);

#endif // __FUSED_H_
