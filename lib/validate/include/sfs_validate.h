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
