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

#ifndef __SSTACK_HELPER_H_
#define __SSTACK_HELPER_H_

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <sstack_nfs.h>
#include <sstack_log.h>
#include <sstack_md.h>
#include <sstack_nfs.h>
#include <openssl/sha.h>
#include <inttypes.h>

#define MAX_INODE_LEN 40 // Maximum number of characters in 2^128 is 39

int32_t sstack_helper_init(log_ctx_t *ctx);
sstack_inode_t* sstack_create_inode(void);
void sstack_free_inode(sstack_inode_t *inode);
int get_inode(unsigned long long , sstack_inode_t *, db_t *);
int put_inode(sstack_inode_t *, db_t *);
inline void sstack_free_erasure(log_ctx_t *, sstack_extent_t *, int );
inline void sstack_free_inode_res(sstack_inode_t *, log_ctx_t *);
uint8_t * create_hash(void *, size_t , uint8_t *, log_ctx_t *);
inline char * get_opt_str(const char *, char *);

#endif //__SSTACK_HELPER_H_
