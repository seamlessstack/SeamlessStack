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

#ifndef __SSTACK_LRU_H__
#define __SSTACK_LRU_H__
#include <inttypes.h>
#include <openssl/sha.h>
#include <sstack_log.h>
#include <red_black_tree.h>

// LRU entry structure
// Key used for the RB-tree node is time()
typedef struct sstack_lru_entry {
	uint8_t hashkey[SHA256_DIGEST_LENGTH + 1];
	size_t len;
	// FIXME:
	// Add fields if needed
} sstack_lru_entry_t;

// LRU functions
extern rb_red_blk_tree *lru_init(log_ctx_t *);
extern int lru_insert_entry(rb_red_blk_tree *, void *, log_ctx_t *);
extern int lru_demote_entries(rb_red_blk_tree *, int , log_ctx_t *);

extern rb_red_blk_tree *lru_tree;

#endif // __SSTACK_LRU_H__
