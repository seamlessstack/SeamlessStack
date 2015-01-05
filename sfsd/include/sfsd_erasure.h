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

#ifndef __SFS_ERASURE_H__
#define __SFS_ERASURE_H__

typedef enum {
        ESURE_SUCCESS = 0,
        ESURE_NO_MEM = -128,
        ESURE_INV_DATA_STRP,
        ESURE_INV_CODE_STRP,
        ESURE_NUM_ERR_STRP_INVLD,
        ESURE_INV_BITMATRIX,
        ESURE_SIZE_MISMATCH,
        ESURE_INIT_FAIL,
        ESURE_ERR
} esure_err_t;

#define CODE_STRIPES 4
#define MAX_DATA_STRIPES 8

int sfs_erasure_init(void);
int sfs_esure_encode(void **data, int num_dstripes, void **code, 
			int num_cstripes, int strp_size);
int sfs_esure_decode(void **data, int num_dstripes, void **code,
                int num_cstripes, int *err_strips,  int num_err,
                 int strp_size);

#endif /* __SFS_ERASURE_H__ */
