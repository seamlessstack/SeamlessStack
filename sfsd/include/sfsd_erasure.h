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

int sfs_erasure_init(void);
int sfs_esure_encode(void **data, int num_dstripes, void **code, 
			int num_cstripes, int strp_size);
int sfs_esure_decode(void **data, int num_dstripes, void **code,
                int num_cstripes, int *err_strips,  int num_err,
                 int strp_size);

#endif /* __SFS_ERASURE_H__ */
