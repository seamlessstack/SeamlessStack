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

/*
 * This implementation is taken from http://c-faq.com/misc/bitsets.html
 */

#ifndef __SSTACK_BITOPS_H__
#define __SSTACK_BITOPS_H__
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sstack_types.h>
#include <sstack_log.h>

#define BITMASK(b) (1 << ((b) % CHAR_BIT))
#define BITSLOT(b) ((b) / CHAR_BIT)
#define BITSET(a, b) ((a)[BITSLOT(b)] |= BITMASK(b))
#define BITCLEAR(a, b) ((a)[BITSLOT(b)] &= ~BITMASK(b))
#define BITTEST(a, b) ((a)[BITSLOT(b)] & BITMASK(b))
#define BITNSLOTS(nb) ((nb + CHAR_BIT - 1) / CHAR_BIT)

/*
 * sfs_init_bitmap - Allocate bitmap and set the bits to 0
 *
 * num_bits - Number of bits in bitmap. Rounded off to next nultiple of 8
 * ctx - log ctx
 *
 * Returns allocated bitmap on success and NULL on failure.
 */
static inline sstack_bitmap_t *
sfs_init_bitmap(int num_bits, log_ctx_t *ctx)
{
	sstack_bitmap_t *bitmap = NULL;

	sfs_log(ctx, SFS_DEBUG, "%s: %d size = %d \n",
					__FUNCTION__, __LINE__, BITNSLOTS(num_bits));

	bitmap = (sstack_bitmap_t *) malloc(BITNSLOTS(num_bits));
	if (NULL == bitmap) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to allocate memory for bitmap \n",
						__FUNCTION__);
		return NULL;
	}
	memset(bitmap, 0x0, BITNSLOTS(num_bits));
	sfs_log(ctx, SFS_DEBUG, "%s: bitmap = 0x%x \n", __FUNCTION__, bitmap);

	return bitmap;
}

#endif // __SSTACK_BITOPS_H__
