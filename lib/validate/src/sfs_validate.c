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
#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sfs_validate.h>

static int elf_encode_hash(char *, char*, int, RSA *);
static void sfs_sha_calculate(char *path, char *hash);

#ifdef TEST_VALIDATE
int
main(void)
{
	char path[100];
	RSA *pub_key, *priv_key;

	strcpy(path,"/home/shank/libabc.so");
	sfs_elf_encode(path, pub_key);
	sfs_elf_validate(path, priv_key);
	return (0);
}
#endif // TEST_VALIDATE


static void
sfs_sha_calculate(char *path, char *hash)
{
	FILE *file = fopen(path, "r");
	SHA256_CTX sha256;
	int buf_size = 32768;
	char *buf = malloc(buf_size * sizeof(char));
	int num_bytes = 0;

	SHA256_Init(&sha256);
	while (num_bytes = fread(buf, 1, buf_size, file)) {
		SHA256_Update(&sha256, buf, num_bytes);
	}

	SHA256_Final(hash, &sha256);

}

int
sfs_elf_encode(char *path, RSA *pub_key)
{
	unsigned char hash[SHA256_DIGEST_LENGTH];
	int res = 0;

	sfs_sha_calculate(path, hash);
	res = elf_encode_hash(hash, path, sizeof(hash), pub_key);

	return (res);
}

static int
elf_encode_hash(char *hash, char *path, int size, RSA *pub_key)
{
	int fd;
	int offset = 0;
	int	num_entries = 0;
	int bytes = 0;
	char enc_code[SHA256_DIGEST_LENGTH];
	int	err;

	err = RSA_public_encrypt(size, hash, enc_code, pub_key,
					RSA_PKCS1_PADDING);
	if (err < 0) {
		return (VLDT_ENCR_FAIL);
	}

	if (( fd = open (path, O_RDWR|O_APPEND , 0777)) < 0) {
        	// Log errno
		return (VLDT_OPEN_FAIL);
	}

	if ((bytes = write(fd, enc_code, size)) < 0) {
		// Log errno
		close (fd);
		return (VLDT_WRITE_FAIL);
	}
	close(fd);

	return (VLDT_SUCCESS);
}

int
sfs_elf_validate(char *path, RSA *priv_key)
{
	int fd, res = 0;
	struct stat sts;
	int size = 0;
	char buf[32];
	off_t off;
	int bytes = 0, i, key_size, err;
	unsigned char hash[SHA256_DIGEST_LENGTH];
	unsigned char signature[SHA256_DIGEST_LENGTH];

	fd = open(path, O_RDWR, 0777);
	if (fd < 0) {
		// Log to print errno
		return (VLDT_OPEN_FAIL);
	}

	fstat(fd, &sts);
	size = sts.st_size;

	if (size < 32) {
		close (fd);
		return (VLDT_SIZE_MISMATCH);
	}

	if ((off = lseek(fd, size-32, SEEK_SET)) < 0) {
		// Log errno
		close (fd);
		return (VLDT_LSEEK_FAIL);
	}

	if ((bytes = read(fd, buf, 32)) < 0) {
		// Log errno
		close (fd);
		return (VLDT_READ_FAIL);
	}

	key_size = RSA_size(priv_key);
	err = RSA_private_decrypt(key_size, buf, signature, priv_key,
					RSA_PKCS1_PADDING);
	if (err < 0) {
		close (fd);
		return (VLDT_DECR_FAIL);
	}

	i = ftruncate(fd, size-32);
	if (i < 0) {
		// LOG to print errno
		close (fd);
		return (VLDT_TRUNC_FAIL);
	}

	sfs_sha_calculate(path, hash);
	if (!memcmp(hash, signature, bytes)) {
		res = VLDT_SUCCESS;
	} else {
		res = VLDT_FAIL;
	}

	off = lseek(fd, 0, SEEK_END);
	bytes = write(fd, buf, size);
	close(fd);

	return (res);
}
