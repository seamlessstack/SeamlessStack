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
#ifndef __SSTACK_NFS_H_
#define __SSTACK_NFS_H_

#include <limits.h>
#include <sstack_storage.h>
#include <policy.h>

/**
  * Commands that are sent from SFS to SFSD.
  * The command list includes the following:-
  * 1) Storage Management Commands
  * 2) NFS Commands
  * 3)...
  *
  **/
typedef enum sstack_command {
	SSTACK_ADD_STORAGE = 1,
	SSTACK_REMOVE_STORAGE = 2,
	SSTACK_UPDATE_STORAGE = 3,
	SSTACK_ADD_STORAGE_RSP = 4,
	SSTACK_REMOVE_STORAGE_RSP = 5,
	SSTACK_UPDATE_STORAGE_RSP = 6,
	SSTACK_MAX_STORAGE_COMMAND = 6,
	NFS_GETATTR = 7,
	NFS_SETATTR = 8,
	NFS_LOOKUP = 9,
	NFS_ACCESS = 10,
	NFS_READLINK = 11,
	NFS_READ = 12,
	NFS_WRITE = 13,
	NFS_CREATE = 14,
	NFS_MKDIR = 15,
	NFS_SYMLINK = 16,
	NFS_MKNOD = 17,
	NFS_REMOVE = 18,
	NFS_RMDIR = 19,
	NFS_RENAME = 20,
	NFS_LINK = 21,
	NFS_READDIR = 22,
	NFS_READDIRPLUS = 23,
	NFS_FSSTAT = 24,
	NFS_FSINFO = 25,
	NFS_PATHCONF = 26,
	NFS_COMMIT = 27,
	NFS_GETATTR_RSP = 28,
	NFS_LOOKUP_RSP = 29,
	NFS_ACCESS_RSP = 30,
	NFS_READLINK_RSP = 31,
	NFS_READ_RSP = 32,
	NFS_WRITE_RSP = 33,
	NFS_CREATE_RSP = 34,
	NFS_VALID_MAX = 34,
} sstack_command_t;

/* Data structures for individual commands */

typedef enum sstack_file_type {
	NFS_REG = 1,
	NFS_DIR = 2,
	NFS_BLK = 3,
	NFS_CHR = 4,
	NFS_LNL = 5,
	NFS_SOCK = 6,
	NFS_FIFO = 7,
} sstack_file_type_t;

typedef struct sstack_file_attribute {
	sstack_file_type_t type;
	uint32_t mode;
	uint32_t num_link;
	uint32_t uid;
	uint32_t gid;
	size_t size;
	size_t used;
	uint64_t fsid;
	uint32_t fileid;
	struct timespec atime;
	struct timespec mtime;
	struct timespec ctime;
} sstack_file_attribute_t;

typedef struct sstack_file_handle {
	size_t name_len;
	char name[PATH_MAX];
	sfs_protocol_t proto;
	sstack_address_t address;
} sstack_file_handle_t;

/* =============== COMMAND STRUCTURES ========================*/

struct sstack_chunk_cmd {
	sfsd_storage_t storage;
};

struct sstack_nfs_setattr_cmd {
	sstack_file_attribute_t attribute;
};

struct sstack_nfs_lookup_cmd {
	sstack_file_handle_t where;
	sstack_file_handle_t what;
};

struct sstack_nfs_read_cmd {
	uint64_t inode_no;
	off_t offset;
	size_t count;
	struct policy_entry pe;
};


struct sstack_nfs_write_cmd {
	uint64_t inode_no;
	off_t offset;
	size_t count;
	struct {
		size_t data_len;
		uint8_t data_val[MAX_EXTENT_SIZE];
	} data;
	struct policy_entry entry;
};

struct sstack_nfs_create_cmd {
	uint64_t inode_no;
	uint32_t mode;
	struct {
		size_t data_len;
		uint8_t data_val[MAX_EXTENT_SIZE];
	} data;
	struct policy_entry entry;
};

struct sstack_nfs_mkdir_cmd {
	/* Name in file handle */
	uint32_t mode;
};
struct sstack_nfs_access_cmd {
	uint32_t mode;
};

struct sstack_nfs_symlink_cmd {
	/* Old name in file handle */
	sstack_file_handle_t new_path;
};

struct sstack_nfs_rename_cmd {
	sstack_file_handle_t new_path;
};

struct sstack_nfs_remove_cmd {
	size_t path_len;
	char *path;
};

struct sstack_nfs_commit_cmd {
	off_t offset;
	size_t count;
};
typedef struct sstack_command_struct {
	sstack_file_handle_t extent_handle;
	union {
		/* Add storage command */
		struct sstack_chunk_cmd add_chunk_cmd;

		/* Update storage command */
		struct sstack_chunk_cmd update_chunk_cmd;

		/* Remove storage commad */
		struct sstack_chunk_cmd delete_chunk_cmd;

		/* ===== NFS COMMANDS BELOW ======= */

		/* NFS v3 GETATTR command -
		 Doesnot need anything apart
		 from file handle */

		/* NFS v3 SETATTR command */
		struct sstack_nfs_setattr_cmd setattr_cmd;

		/* NFS v3 LOOKUP command */
		struct sstack_nfs_lookup_cmd lookup_cmd;

		/* NFS v3 ACCESS command */
		struct sstack_nfs_access_cmd access_cmd;

		/* NFS v3 READ command */
		struct sstack_nfs_read_cmd read_cmd;

		/* NFS v3 WRITE command */
		struct sstack_nfs_write_cmd write_cmd;

		/* NFS v3 CREATE command */
		struct sstack_nfs_create_cmd create_cmd;

		/* NFS v3 MKDIR command */
		struct sstack_nfs_mkdir_cmd mkdir_cmd;

		struct sstack_nfs_symlink_cmd symlink_cmd;

		/* NFS v3 RENAME command */
		struct sstack_nfs_rename_cmd rename_cmd;

		/* NFS v3 REMOVE command */
		struct sstack_nfs_remove_cmd remove_cmd;

		/* NFS v3 commit command */
		struct sstack_nfs_commit_cmd commit_cmd;
	};

} sstack_nfs_command_struct;

/* ============= RESPONSE STRUCTURES ==========================*/

struct sstack_nfs_getattr_resp {
	struct stat stbuf;
};

struct sstack_nfs_lookup_resp {
	size_t lookup_path_len;
	char *lookup_path;
};

struct sstack_nfs_access_resp {
	uint32_t access;
};

struct sstack_nfs_read_resp {
	size_t count;
	int32_t eof;
	struct {
		size_t data_len;
		uint8_t data_val[MAX_EXTENT_SIZE];
	} data;
};

struct sstack_nfs_write_resp {
	uint32_t file_create_ok;
	uint32_t file_wc;
};

struct sstack_nfs_readlink_resp {
	sstack_file_handle_t real_file;
};

typedef struct sstack_nfs_response_struct {
	int32_t command_ok;
	union {
		/* NFS v3 GETATTR response */
		struct sstack_nfs_getattr_resp getattr_resp;

		/* NFS v3 SETATTR response */
		/* Just update the command status */

		/* NFS v3 LOOKUP response */
		struct sstack_nfs_lookup_resp lookup_resp;

		/* NFS v3 ACCESS response */
		struct sstack_nfs_access_resp access_resp;

		/* NFS v3 READLINK response */
		struct sstack_nfs_readlink_resp readlink_resp;

		/* NFS v3 READ response */
		struct sstack_nfs_read_resp read_resp;

		/* NFS v3 WRITE response */
		struct sstack_nfs_write_resp write_resp;

		/* NFS v3 CREATE response */
		struct sstack_nfs_write_resp create_resp;

		/* NFS v3 REMOVE response */
		/* Just update the command status */

		/* NFS v3 RENAME response */
		/* Just update the command status */

		/* NFS v3 COMMIT response */
		/* Need to fill in later */
	};

} sstack_nfs_response_struct;

#endif /* __SSTACK_NFS_H_ */
