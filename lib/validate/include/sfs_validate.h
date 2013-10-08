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

#ifndef __SFS_VALIDATE_H__
#define __SFS_VALIDATE_H__

#include <openssl/rsa.h>

typedef enum {
	VLDT_ENCR_FAIL = -128,
	VLDT_OPEN_FAIL,
	VLDT_WRITE_FAIL,
	VLDT_SIZE_MISMATCH,
	VLDT_LSEEK_FAIL,
	VLDT_READ_FAIL,
	VLDT_DECR_FAIL,
	VLDT_TRUNC_FAIL,
	VLDT_FAIL = -1,
	VLDT_SUCCESS
} vldt_err_t;

/* Function to encode validate info and add to the elf file
	Input:
		path = path of the file on the build server
		pub_key = public key generated(need to generate
				(public,private) key pair before 
				calling this API
	Return:
		On success, returns VLDT_SUCCESS
		On failure, returns one of the above failures
*/		
extern int sfs_elf_encode(char *path, RSA *pub_key);


/* Function to validate the elf 
	Input:
		path = path of the file at deployment site
		priv_key = private key generated

	Returns:
		On validation success, returns VLDT_SUCCESS
		On failure, returns one of the above failures
*/
extern int sfs_elf_validate(char *path, RSA *priv_key);

#endif /* __SFS_VALIDATE_H__ */
