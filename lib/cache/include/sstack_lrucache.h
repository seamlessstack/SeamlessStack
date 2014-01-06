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
	// FIXME:
	// Add fields if needed
} sstack_lru_entry_t;

typedef int (*cache_purge_func_t) (uint8_t *, log_ctx_t *);

// LRU functions
extern rb_red_blk_tree *lru_init(log_ctx_t *);
extern int lru_insert_entry(rb_red_blk_tree *, void *, log_ctx_t *);
extern int lru_delete_entry(rb_red_blk_tree *, int , cache_purge_func_t ,
				log_ctx_t *);

#endif // __SSTACK_LRU_H__


