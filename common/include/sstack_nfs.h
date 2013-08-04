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

typedef enum sstack_nfs_command {
	NFS_GETATTR = 1,
	NFS_SETATTR,
	NFS_LOOKUP,
	NFS_ACCESS,
	NFS_READLINK,
	NFS_READ,
	NFS_WRITE,
	NFS_CREATE,
	NFS_MKDIR,
	NFS_SYMLINK,
	NFS_MKNOD,
	NFS_REMOVE,
	NFS_RMDIR,
	NFS_RENAME,
	NFS_LINK,
	NFS_READDIR,
	NFS_READDIRPLUS,
	NFS_FSSTAT,
	NFS_FSINFO,
	NFS_PATHCONF,
	NFS_COMMIT,
	NFS_VALID_MAX,
} sstack_nfs_command_t;

typedef struct sstack_command_struct {
	/* Existing file handle */
	char file_handle[PATH_MAX];
	union {
		/* NFS v3 READ command */
		struct nfs_read_cmd {
			off_t offset;
			size_t count;
		} read_cmd;

		/* NFS v3 WRITE command */
		struct nfs_write_cmd {
			off_t offset;
			size_t count;
			struct {
				size_t data_len;
				uint8_t data_val[MAX_EXTENT_SIZE];
			} data;
		} write_cmd;

		/* NFS v3 CREATE command */
		struct nfs_create_cmd {
			uint32_t mode;
			struct {
				size_t data_len;
				uint8_t data_val[MAX_EXTENT_SIZE];
			} data;
		} create_cmd;

		/* NFS v3 ACCESS command */
		struct nfs_access_cmd {
			uint32_t mode;
		} access_cmd;

		/* NFS v3 RENAME command */
		struct nfs_rename_cmd {
			char new_path[PATH_MAX];
		} rename_cmd;

		/* NFS v3 commit command */
		struct nfs_commit_cmd {
			off_t offset;
			size_t count;
		} commit_cmd;
	};

} sstack_nfs_command_struct;

typedef struct sstack_nfs_result_struct {
	union {
		/* NFS v3 LOOKUP response */
		struct nfs_lookup_resp {
			char lookup_path[PATH_MAX];
			/* Attributes if required */
		} lookup_resp;

		/* NFS v3 ACCESS response */
		struct nfs_access_resp {
			uint32_t access;
		} access_resp;

		/* NFS v3 READ response */
		struct nfs_read_resp {
			size_t count;
			int32_t eof;
			struct {
				size_t data_len;
				uint8_t data_val[MAX_EXTENT_SIZE];
			} data;
		} read_resp;

		/* NFS v3 WRITE response */
		struct nfs_write_resp {
			uint32_t file_write_ok;
			uint32_t file_wc;
		} write_resp;

		/* NFS v3 CREATE response */
		struct nfs_create_resp {
			uint32_t file_create_ok;
			uint32_t file_wc;
		} create_resp;

		/* NFS v3 REMOVE response */
		struct nfs_remove_resp {
			uint32_t file_remove_ok;
		} remove_resp;

		/* NFS v3 RENAME response */
		struct nfs_rename_resp {
			uint32_t file_rename_ok;
		} rename_resp;

		/* NFS v3 FSSTAT response */
		struct nfs_fsstat_resp {
			/* Need to fill */
		} fsstat_resp;

		/* NFS v3 FSINFO response */
		struct nfs_fsinfo_resp {
			/* Need to fill */
		} fsinfo_resp;

		/* NFS v3 PATHCONF response */
		struct nfs_pathconf_resp {
			/* Need to fill */
		} pathconf_resp;

		/* NFS v3 COMMIT response */
		struct nfs_commit_resp {
			/* Need to fill */
		} commit_resp;
	};

} sstack_nfs_result_struct;

#endif /* __SSTACK_NFS_H_ */
