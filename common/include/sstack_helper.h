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
int put_inode(sstack_inode_t *, db_t *, int );
inline void sstack_free_erasure(log_ctx_t *, sstack_extent_t *, int );
inline void sstack_free_inode_res(sstack_inode_t *, log_ctx_t *);
uint8_t * create_hash(void *, size_t , uint8_t *, log_ctx_t *);
inline char * get_opt_str(const char *, char *);
void sstack_dump_inode(sstack_inode_t *inode, log_ctx_t *ctx);
void sstack_dump_extent(sstack_extent_t *ex, log_ctx_t *ctx);

#endif //__SSTACK_HELPER_H_
