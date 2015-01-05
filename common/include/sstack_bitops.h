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
