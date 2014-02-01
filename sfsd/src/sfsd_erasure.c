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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "jerasure.h"
#include "galois.h"
#include <sfsd_erasure.h>

#define NUM_PKT_STRIPS	8
#define PACKET_SIZE(size)	(size/NUM_PKT_STRIPS)
#define MAX_ERROR_STRIPES	CODE_STRIPES

/* Bitmatrices derived upfront from GF(2^8) */
static int *bitmatrix_2; /* For 2 data stripes and 2 code stripes */
static int *bitmatrix_3; /* For 3 data stripes and 3 code stripes */
static int *bitmatrix_4; /* For 4 data stripes and 4 code stripes */
static int *bitmatrix_5; /* For 5 data stripes and 4 code stripes */
static int *bitmatrix_6; /* For 6 data stripes and 4 code stripes */
static int *bitmatrix_7; /* For 7 data stripes and 4 code stripes */
static int *bitmatrix_8; /* For 8 data stripes and 4 code stripes */

static int
sfs_derive_bitmatrix(int **bmatrix, int num_dstrps, int num_cstrps)
{
	int *matrix;
	int i,j;

	matrix = (int *)malloc(sizeof(int) * (num_dstrps) * (num_cstrps));
	if (matrix == NULL) {
		return (ESURE_NO_MEM);
	}

	for (i = 0; i < num_cstrps; i++) {
		for (j = 0; j < num_dstrps; j++) {
			matrix[i*num_dstrps+j] =
				galois_single_divide(1, i ^ (num_cstrps + j), NUM_PKT_STRIPS);
		}
	}

	*bmatrix = jerasure_matrix_to_bitmatrix(num_dstrps, num_cstrps,
					NUM_PKT_STRIPS, matrix);
	if (*bmatrix == NULL) {
		return (ESURE_INV_BITMATRIX);
	}

	return (ESURE_SUCCESS);
}

int
sfs_erasure_init(void)
{
	int ret = 0;

	ret = sfs_derive_bitmatrix(&bitmatrix_2, 2, 2);
	if (ret < 0) {
		return (ret);
	}

	ret = sfs_derive_bitmatrix(&bitmatrix_3, 3, 3);
	if (ret < 0) {
		return (ret);
	}

	ret = sfs_derive_bitmatrix(&bitmatrix_3, 4, 4);
	if (ret < 0) {
		return (ret);
	}

	ret = sfs_derive_bitmatrix(&bitmatrix_3, 5, 4);
	if (ret < 0) {
		return (ret);
	}

	ret = sfs_derive_bitmatrix(&bitmatrix_3, 6, 4);
	if (ret < 0) {
		return (ret);
	}

	ret = sfs_derive_bitmatrix(&bitmatrix_3, 7, 4);
	if (ret < 0) {
		return (ret);
	}

	ret = sfs_derive_bitmatrix(&bitmatrix_3, 8, 4);
	if (ret < 0) {
		return (ret);
	}

	return (ESURE_SUCCESS);
}

static int
sfs_data_stripes_encode_1(void **data, void **code, int size)
{
	int i = 0;

	/* storage optimization for very small files */
	if (size < 256) {
		for (i = 0; i < CODE_STRIPES/2; i++) {
			memcpy((char *)code[i], (char *)(*data), size);
		}
		return (CODE_STRIPES/2);
	} else {
		for (i = 0; i < CODE_STRIPES; i++) {
			memcpy((char *)code[i], (char *)(*data), size);
		}
		return (CODE_STRIPES);
	}
}

static int
sfs_data_stripes_encode_2(void **data, void **code, int size)
{
	int i = 0;

	jerasure_bitmatrix_encode(2, 2, NUM_PKT_STRIPS, bitmatrix_2, (char **) data,
			(char **) code, size, PACKET_SIZE(size));

	for (i = 0; i < CODE_STRIPES/2; i++) {
		memcpy((char *)code[CODE_STRIPES/2 + i], (char *)code[i], size);
	}

	return (CODE_STRIPES);
}

static int
sfs_data_stripes_encode_3(void **data, void **code, int size)
{
	int i = 0;

	jerasure_bitmatrix_encode(3, 3, NUM_PKT_STRIPS, bitmatrix_3, (char **) data,
				(char **) code, size, PACKET_SIZE(size));

	memcpy((char *)code[CODE_STRIPES - i], (char *)data[0], size);

	return (CODE_STRIPES);
}

static int
sfs_data_stripes_encode_gen(void **data, void **code, int num_stripes,
				int size)
{
	int	*bmatrix = NULL;

	switch (num_stripes) {
		case 4:
			bmatrix = bitmatrix_4;
			break;
		case 5:
			bmatrix = bitmatrix_5;
			break;
		case 6:
			bmatrix = bitmatrix_6;
			break;
		case 7:
			bmatrix = bitmatrix_7;
			break;
		case 8:
			bmatrix = bitmatrix_8;
			break;
		default:
			return (-1);
	}

	jerasure_bitmatrix_encode(num_stripes, 4, NUM_PKT_STRIPS, bmatrix,
		(char **) data, (char **) code, size, PACKET_SIZE(size));

	return (CODE_STRIPES);
}

int
sfs_esure_encode(void **data, int num_dstripes, void **code,
			int num_cstripes, int strp_size)
{
	int size, i=0;
	int ret = 0;

	if (num_dstripes >  MAX_DATA_STRIPES || num_dstripes <=0) {
		return (ESURE_INV_DATA_STRP);
	}

	if (num_cstripes != CODE_STRIPES) {
		return (ESURE_INV_CODE_STRP);
	}

	if (PACKET_SIZE(strp_size) % sizeof(long))
		return (ESURE_SIZE_MISMATCH);

	if (data[0] == NULL) {
		return (ESURE_INV_DATA_STRP);
	}
	size = sizeof(data[0]);
	for (i = 1; i < num_dstripes; i++) {
		if (!data[i] || (size != sizeof(data[i])))
			return (ESURE_INV_DATA_STRP);
	}
	for (i = 0; i < num_cstripes; i++) {
		if (!code[i] || (size != sizeof(code[i])))
			return (ESURE_INV_CODE_STRP);
	}

	switch (num_dstripes) {
		case 1:
			ret = sfs_data_stripes_encode_1(data, code, strp_size);
			break;
		case 2:
			ret = sfs_data_stripes_encode_2(data, code, strp_size);
			break;
		case 3:
			ret = sfs_data_stripes_encode_3(data, code, strp_size);
			break;
		case 4:
		case 5:
		case 6:
		case 7:
		case 8:
			ret = sfs_data_stripes_encode_gen(data, code,
							num_dstripes, strp_size);
			break;
		default:
			return (-1);
	}

	return (ret);
}

int
sfs_esure_decode(void **data, int num_dstripes, void **code,
		int num_cstripes, int *err_strips,  int num_err,
		 int strp_size)
{
	int size, i = 0, j = 0;
	int *erasures, *bmatrix;

	if (num_dstripes >  MAX_DATA_STRIPES || num_dstripes <=0) {
		return (ESURE_INV_DATA_STRP);
	}

	if (num_cstripes != CODE_STRIPES) {
		return (ESURE_INV_CODE_STRP);
	}
	if (strp_size < 256 && num_cstripes != CODE_STRIPES/2)
		return (ESURE_INV_CODE_STRP);
	if (strp_size < 256 &&  num_err > MAX_ERROR_STRIPES/2)
		return (ESURE_NUM_ERR_STRP_INVLD);
	if (num_err < 0 || num_err > MAX_ERROR_STRIPES)
		return (ESURE_NUM_ERR_STRP_INVLD);
	if (num_err== 0)
		return ESURE_SUCCESS;
	if (PACKET_SIZE(strp_size) % sizeof(long))
		return (ESURE_SIZE_MISMATCH);

	if (data[0] == NULL) {
		return (ESURE_INV_DATA_STRP);
	}
	size = sizeof(data[0]);
	for (i = 1; i < num_dstripes; i++) {
		if (!data[i] || (size != sizeof(data[i])))
			return (ESURE_INV_DATA_STRP);
	}
	for (i = 0; i < num_cstripes; i++) {
		if (!code[i] || (size != sizeof(code[i])))
			return (ESURE_INV_CODE_STRP);
	}

	for (i = 0; i < num_err; i++) {
		if (err_strips[i] < 0 ||
				err_strips[i] > (num_dstripes + num_cstripes))
			return (ESURE_ERR);

		if (err_strips[i] < num_dstripes)
			memset(data[err_strips[i]], strp_size, 0);
		else
			memset(code[err_strips[i]], strp_size, 0);
	}

	erasures = (int *)malloc(sizeof(int) * (num_err+1));
	switch (num_dstripes) {
		case 1: {
			int strps = 0;
			char *ptr;

			for (i = 0; i < num_err; i++) {
				strps |= (1 << err_strips[i]);
			}
			for (i = 0; i < (num_dstripes + num_cstripes); i++) {
				if (!(strps & 1<<i))
					break;
			}
			ptr = (i == 0) ? (char *)data[0] : (char *)code[i-1];
			memcpy((char *)data[0], ptr, strp_size);
			for (i = 0; i < num_cstripes; i++) {
				memcpy((char *)code[i], ptr, strp_size);
			}
			free(erasures);
			return (ESURE_SUCCESS);
		}
		case 2: {
			int strps = 0;

			for (i = 0; i < num_err; i++) {
				strps |= (1 << err_strips[i]);
			}
			if ((strps & 0x2B) || (strps & 0x17)) {
				free(erasures);
				return (ESURE_ERR);
			}

			for (i = 0; i < num_err; i++) {
				if (err_strips[i] == (num_dstripes + num_cstripes -1))
					continue;
				erasures[j++] = err_strips[i];
			}
			erasures[j] = -1;
			bmatrix = bitmatrix_2;
 			jerasure_bitmatrix_decode(num_dstripes, num_cstripes-2,
					 NUM_PKT_STRIPS, bmatrix, 0, erasures, (char **) data,
					 (char **) code, strp_size, PACKET_SIZE(strp_size));
			for (i = 0; i < CODE_STRIPES/2; i++) {
		         memcpy((char *)code[CODE_STRIPES/2 + i],
                                (char *)code[i], size);
 			}

			return (ESURE_SUCCESS);
		}
		case 3: {
			for (i = 0; i < num_err; i++) {
				if (err_strips[i] == (num_dstripes + num_cstripes -1))
					continue;
				erasures[j++] = err_strips[i];
			}
			erasures[j] = -1;
			bmatrix = bitmatrix_3;
 			jerasure_bitmatrix_decode(num_dstripes, num_cstripes-1,
				 NUM_PKT_STRIPS, bmatrix, 0, erasures, (char **) data,
				 (char **) code, strp_size, PACKET_SIZE(strp_size));
			memcpy((char *)code[CODE_STRIPES-1], data[0], strp_size);
			return (ESURE_SUCCESS);
		}
        case 4:
                bmatrix = bitmatrix_4;
                break;
        case 5:
                bmatrix = bitmatrix_5;
                break;
        case 6:
                bmatrix = bitmatrix_6;
                break;
        case 7:
                bmatrix = bitmatrix_7;
                break;
        case 8:
                bmatrix = bitmatrix_8;
                break;
        default:
			return (ESURE_ERR);
 	}

	for (i = 0; i < num_err; i++) {
		if (err_strips[i] == (num_dstripes + num_cstripes -1))
			continue;
		erasures[j++] = err_strips[i];
	}
	erasures[j] = -1;
	jerasure_bitmatrix_decode(num_dstripes, num_cstripes, NUM_PKT_STRIPS,
			bmatrix, 0, erasures, (char **) data, (char **) code, strp_size,
			PACKET_SIZE(strp_size));

	return (ESURE_SUCCESS);
}

#if VALIDATE_JERASURE
int
main(void)
{
	return (0);
}
#endif // VALIDATE_JERASURE
