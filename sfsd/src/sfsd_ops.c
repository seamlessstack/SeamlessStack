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

#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sstack_types.h>
#include <sstack_nfs.h>
#include <sstack_jobs.h>
#include <sstack_chunk.h>
#include <sstack_sfsd.h>
#include <bds_slab.h>
#include <sstack_md.h>
#include <sstack_helper.h>
#include <sstack_signatures.h>
#include <sfsd_erasure.h>

#define USE_INDEX 0
#define USE_HANDLE 1
#define ERASURE_STRIPE_SIZE (64*1024)

extern bds_cache_desc_t sfsd_global_cache_arr[];
extern char *get_mount_path(sfs_chunk_domain_t * , sstack_file_handle_t * ,
				char ** );
extern int32_t match_address(sstack_address_t * , sstack_address_t * );
extern sfsd_t sfsd;

sstack_payload_t* sstack_getattr(sstack_payload_t *payload, log_ctx_t *ctx)
{

	int32_t command_stat = 0;
	sstack_file_handle_t *file_handle =
		&payload->command_struct.extent_handle;
	char *p = file_handle->name;
	sstack_payload_t *response = NULL;
	struct sstack_nfs_getattr_resp *getattr_resp = NULL;

	/* Make the string null terminated */
	p[file_handle->name_len - 1] = 0;
	sfs_log(ctx, SFS_DEBUG, ":%s extent path:%s", __FUNCTION__, p);

	//i= find_rorw_branch(path); - Might need later
	response = bds_cache_alloc(sfsd_global_cache_arr[PAYLOAD_CACHE_OFFSET]);
	if (!response) {
		command_stat = -ENOMEM;
		goto error;
	}

	getattr_resp = &response->response_struct.getattr_resp;
	command_stat = lstat(p, &getattr_resp->stbuf);
	sfs_log(ctx, SFS_DEBUG, "%s(): command status: %d", __FUNCTION__,
			command_stat);
	if (command_stat != 0) {
		goto error;
	}

	/* This is a workaround for broken gnu find implementations. Actually,
	 * n_links is not defined at all for directories by posix. However, it
	 * seems to be common for filesystems to set it to one if the actual
	 * value is unknown. Since nlink_t is unsigned and since these broken
	 * implementations always substract 2 (for . and ..) this will cause an
	 * underflow, setting it to max(nlink_t).
	 */
	if (S_ISDIR(getattr_resp->stbuf.st_mode))
		getattr_resp->stbuf.st_nlink = 1;

	/* Command is successful */
	response->hdr.sequence = payload->hdr.sequence;
	response->hdr.payload_len = sizeof(*response);
	response->response_struct.command_ok = command_stat;

	return response;

error:
	payload->response_struct.command_ok = command_stat;
	if (response)
		bds_cache_free(sfsd_global_cache_arr[PAYLOAD_CACHE_OFFSET],
				response);
	return payload;
}


sstack_payload_t *sstack_setattr(sstack_payload_t *payload, log_ctx_t *ctx)
{
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented", __FUNCTION__);
	return NULL;
}

sstack_payload_t *sstack_lookup(sstack_payload_t *payload, log_ctx_t *ctx)
{
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented", __FUNCTION__);
	return NULL;
}

sstack_payload_t *sstack_access(sstack_payload_t *payload, log_ctx_t *ctx)
{
	int command_stat = 0;
	struct sstack_nfs_access_cmd *cmd = &payload->command_struct.access_cmd;
	sstack_file_handle_t *file_handle =
		&payload->command_struct.extent_handle;
	sstack_payload_t *response = NULL;
	struct sstack_nfs_access_resp *access_resp;
	char *p = file_handle->name;
	p[file_handle->name_len - 1] = '\0';
	sfs_log(ctx, SFS_DEBUG, "%s(): path", __FUNCTION__, p);

	response = bds_cache_alloc(sfsd_global_cache_arr[PAYLOAD_CACHE_OFFSET]);
	if (!response) {
		sfs_log(ctx, SFS_ERR, "%s(): Memory allocation failed",
				__FUNCTION__);
		command_stat = -ENOMEM;
		goto error;
	}
	access_resp = &response->response_struct.access_resp;
	command_stat = access(p, cmd->mode);
	if (command_stat >= 0) {
		access_resp->access = command_stat;
	} else {
		goto error;
	}
	sfs_log(ctx, SFS_DEBUG, "%s(): command status: %d", __FUNCTION__,
			command_stat);
	/* Command is successful */
	response->hdr.sequence = payload->hdr.sequence;
	response->hdr.payload_len = sizeof(*response);
	response->response_struct.command_ok = command_stat;

	return response;
error:
	payload->response_struct.command_ok = command_stat;
	if (response)
		bds_cache_free(sfsd_global_cache_arr[PAYLOAD_CACHE_OFFSET],
				response);
	return payload;
}


sstack_payload_t *sstack_readlink(sstack_payload_t *payload, log_ctx_t *ctx)
{
	int32_t command_stat = 0;
	sstack_file_handle_t *link = &payload->command_struct.extent_handle;
	sstack_payload_t *response = NULL;
	struct sstack_nfs_readlink_resp *readlink_resp = NULL;
	char *p = link->name;
	p[link->name_len - 1] = '\0';

	response = bds_cache_alloc(sfsd_global_cache_arr[PAYLOAD_CACHE_OFFSET]);
	if (!response) {
		sfs_log(ctx, SFS_ERR, "%s(): Memory allocation failed",
				__FUNCTION__);
		command_stat = -ENOMEM;
		goto error;
	}
	readlink_resp = &response->response_struct.readlink_resp;
	command_stat = readlink(p, readlink_resp->real_file.name, PATH_MAX);
	if (command_stat >= 0) {
		readlink_resp->real_file.name_len = command_stat;
	} else {
		goto error;
	}
	sfs_log(ctx, SFS_DEBUG, "%s(): command status: %d", __FUNCTION__,
			command_stat);
	/* Command is successful */
	response->hdr.sequence = payload->hdr.sequence;
	response->hdr.payload_len = sizeof(*response);
	response->response_struct.command_ok = command_stat;

	return response;

error:
	payload->response_struct.command_ok = command_stat;
	if (response)
		bds_cache_free(sfsd_global_cache_arr[PAYLOAD_CACHE_OFFSET],
				response);
	return payload;

}

/* Match the extent handles based on the protocol and the path
   in the chunk */
static inline int32_t match_extent(sstack_file_handle_t *h1,
				   sstack_file_handle_t *h2)
{
	/* Match extent addresses based on protocol */
	if (h1->proto != h2->proto)
		return FALSE;

	if (match_address(&h1->address, &h2->address) == FALSE)
		return FALSE;

	if (!strncmp(h1->name, h2->name, PATH_MAX))
		return TRUE;

	return FALSE;
}

static int32_t read_and_check_extent(sstack_file_handle_t *extent_handle,
			int32_t index, int32_t use_flag, sstack_inode_t *inode, 
			void *buffer, int32_t chk_cksum, log_ctx_t *ctx)
{
	int32_t fd;
	int32_t ret = 0, nbytes = 0, i = 0, len = 0;
	sstack_extent_t *extent = NULL;
	char *p = extent_handle->name;
	char *q = NULL;
	char *r;
	char *path;
	p[extent_handle->name_len - 1] = '\0';

	if (use_flag == USE_INDEX) {
		extent = &inode->i_extent[index];
	}
	
	/* Search the IP address in the extent handle in the inode
	   structure. Needed to find out the extent_t to get the full
	   information about the particular extent */
	if (use_flag == USE_HANDLE) {
		for(i = 0; i < inode->i_numextents; ++i) {
			if (TRUE == match_extent(extent_handle,
					  inode->i_extent[i].e_path)) {
				extent = &inode->i_extent[i];
				break;
			}
		}
	}

	if (extent == NULL) {
		sfs_log(ctx, SFS_ERR, "%s(): Error reading extent\n");
		ret = -EINVAL;
		goto ret;
	}

	if ((q = get_mount_path(sfsd.chunk, extent_handle, &r)) == NULL) {
		sfs_log(ctx, SFS_ERR, "%s(): Invalid mount path for:%s\n", p);
		ret = -EINVAL;
		goto ret;
	}

	/* Create the file path relative to the mount point */
	len = strlen(r); /* r contains the nfs export */
	p += len; /* p points to start of the path minus the nfs export */
	path = bds_cache_alloc(sfsd_global_cache_arr[DATA4K_CACHE_OFFSET]);
	if (path == NULL) {
		ret = -ENOMEM;
		sfs_log(ctx, SFS_ERR, "%s(): Error getting from path cache",
			__FUNCTION__);
		goto ret;
	}
	sprintf (path, "%s%s",q, p);
	if ((fd = open(path, O_RDONLY)) < 0) {
		sfs_log(ctx, SFS_ERR, "%s(): extent '%s' open failed",
			__FUNCTION__, p);
		ret = errno;;
		goto ret;
	}
	nbytes = read(fd, buffer, extent->e_size);
	if (nbytes == 0) {
		sfs_log(ctx, SFS_ERR, "%s(): Read returned 0\n",
			__FUNCTION__);
		ret = errno;
		goto ret;
	}
	
	if (chk_cksum) {
		uint32_t crc;
		crc = crc_pcl((const char *)buffer, nbytes, 0);
		if (crc != extent->e_cksum) {
			ret = -SSTACK_ECKSUM;
		} else {
			ret = 0;
		}
	} 
ret:
	return ret;
}

static int32_t read_and_check_erasure(int32_t index, sstack_inode_t *inode, 
			void *buffer, int32_t chk_cksum, log_ctx_t *ctx)
{
	int32_t fd;
	int32_t ret = 0, nbytes = 0, len = 0;
	sstack_erasure_t *erasure = NULL;
	char *p = NULL;
	char *q = NULL;
	char *r;
	char *path;

	erasure = &inode->i_erasure[index];
	
	if (erasure == NULL) {
		sfs_log(ctx, SFS_ERR, "%s(): Error reading erasure\n");
		ret = -EINVAL;
		goto ret;
	}
	
	p = erasure->er_path->name;
	p[erasure->er_path->name_len - 1] = '\0';
	if ((q = get_mount_path(sfsd.chunk, erasure->er_path, &r)) == NULL) {
		sfs_log(ctx, SFS_ERR, "%s(): Invalid mount path for:%s\n", p);
		ret = -EINVAL;
		goto ret;
	}

	/* Create the file path relative to the mount point */
	len = strlen(r); /* r contains the nfs export */
	p += len; /* p points to start of the path minus the nfs export */
	path = bds_cache_alloc(sfsd_global_cache_arr[DATA4K_CACHE_OFFSET]);
	if (path == NULL) {
		ret = -ENOMEM;
		sfs_log(ctx, SFS_ERR, "%s(): Error getting from path cache",
			__FUNCTION__);
		goto ret;
	}
	sprintf (path, "%s%s",q, p);
	if ((fd = open(path, O_RDONLY)) < 0) {
		sfs_log(ctx, SFS_ERR, "%s(): extent '%s' open failed",
			__FUNCTION__, p);
		ret = errno;;
		goto ret;
	}
	nbytes = read(fd, buffer, ERASURE_STRIPE_SIZE);
	if (nbytes == 0) {
		sfs_log(ctx, SFS_ERR, "%s(): Read returned 0\n",
			__FUNCTION__);
		ret = errno;
		goto ret;
	}
	
	if (chk_cksum) {
		uint32_t crc;
		crc = crc_pcl((const char *)buffer, nbytes, 0);
		if (crc != erasure->er_cksum) {
			ret = -SSTACK_ECKSUM;
		} else {
			ret = 0;
		}
	} 
ret:
	return ret;
}

sstack_payload_t *sstack_read(sstack_payload_t *payload,
	sfsd_t *sfsd, log_ctx_t *ctx)
{
	int32_t i, command_stat = 0;
	sstack_inode_t *inode;
	struct sstack_nfs_read_cmd *cmd = &payload->command_struct.read_cmd;
	struct policy_entry *pe = &cmd->pe;
	sstack_payload_t *response;
	void *buffer1 = NULL, *buffer2 = NULL;
	inode = bds_cache_alloc(sfsd_global_cache_arr[INODE_CACHE_OFFSET]);
	if (inode == NULL) {
		command_stat = -ENOMEM;
		sfs_log(ctx, SFS_ERR, "%s(): %s\n",
			__FUNCTION__, "Inode cache mem not available");
		goto error;
	}

	if (get_inode(cmd->inode_no, inode, sfsd->db) != 0) {
		command_stat = -EINVAL;
		sfs_log(ctx, SFS_ERR, "%s(): Inode not available",
			__FUNCTION__);
		goto error;
	}

	/* TODO: Make it a function */
	if (cmd->read_ecode) {
		/* Read erasure coded data and send response */
		void	*d_stripes[MAX_DATA_STRIPES];
		void	*c_stripes[CODE_STRIPES];
		int32_t index = 0, er_index = 0,  final_dstripe_idx;
		int32_t err_strps[MAX_DATA_STRIPES + CODE_STRIPES];
		int32_t num_err_strps = 0, num_dstripes = 0;
		int32_t ecode_idx = 0, ret = 0;

		for(i = 0; i < inode->i_numextents; ++i) {
			if (TRUE == match_extent(&payload->command_struct.extent_handle,
					  inode->i_extent[i].e_path)) {
				index = i;
				break;
			}
		}
		er_index = ((index/MAX_DATA_STRIPES) * CODE_STRIPES);
		ecode_idx = (index % MAX_DATA_STRIPES);
		index -= ecode_idx;
		if ((index + MAX_DATA_STRIPES) > inode->i_numextents) {
			final_dstripe_idx = inode->i_numextents;
		} else {
			final_dstripe_idx = (index + MAX_DATA_STRIPES);
		}
		
		for (i = index; i < final_dstripe_idx; i++) {
			d_stripes[i-index] = 
				bds_cache_alloc(sfsd_global_cache_arr[DATA64K_CACHE_OFFSET]);
			if (d_stripes[i-index] == NULL) {
				sfs_log(ctx, SFS_ERR, "%s(): Error getting read buffer\n",
					__FUNCTION__);
				command_stat = -ENOMEM;
				goto error;
			}
			ret = read_and_check_extent(NULL, i, USE_INDEX, inode, 
					d_stripes[i-index], 1, ctx);
			if (ret == -SSTACK_ECKSUM) {
				err_strps[num_err_strps++] = (i-index);
			}
			num_dstripes++;
		}	
		
		for (i = er_index; i < er_index + CODE_STRIPES; i++) {
			c_stripes[i-er_index] = 
				bds_cache_alloc(sfsd_global_cache_arr[DATA64K_CACHE_OFFSET]);
			if (c_stripes[i-er_index] == NULL) {
				sfs_log(ctx, SFS_ERR, "%s(): Error getting read buffer\n",
					__FUNCTION__);
				command_stat = -ENOMEM;
				goto error;
			}
			ret = read_and_check_erasure(i, inode, c_stripes[i-er_index], 
												1, ctx);
			if (ret == -SSTACK_ECKSUM) {
				err_strps[num_err_strps++] = (i-er_index);
			}
		}
			
		ret = sfs_esure_decode(d_stripes, num_dstripes, c_stripes, CODE_STRIPES,
								err_strps, num_err_strps, ERASURE_STRIPE_SIZE);
	}
		
	/* Now time for reading. Allocate a repsonse structure */
	response = bds_cache_alloc(sfsd_global_cache_arr[PAYLOAD_CACHE_OFFSET]);
	if (response == NULL) {
		command_stat = -ENOMEM;
		sfs_log(ctx, SFS_ERR, "%s(): No memory for response struct\n",
			__FUNCTION__);
		goto error;
	}
	buffer1 = bds_cache_alloc(sfsd_global_cache_arr[DATA64K_CACHE_OFFSET]);
	if (buffer1 == NULL) {
		sfs_log(ctx, SFS_ERR, "%s(): Error getting read buffer\n",
			__FUNCTION__);
		command_stat = -ENOMEM;
		goto error;
	}
	/* Read the actual extent now */
	command_stat = read_and_check_extent(&payload->command_struct.extent_handle,
				   0, USE_HANDLE, inode, buffer1, 1, ctx);
	if (command_stat != 0) {
		sfs_log(ctx, SFS_ERR, "%s(): Error reading extent",
			__FUNCTION__);
		goto error;
	}

	for(i = pe->pe_num_plugins; i >= 0; i--) {
		//Call the remove plugins from the dlsym/dlopen
		//Finally buffer2 should have the data
	}
	
	response = bds_cache_alloc(sfsd_global_cache_arr[PAYLOAD_CACHE_OFFSET]);
	if (!response) {
		sfs_log(ctx, SFS_ERR, "%s(): Memory allocation failed",
				__FUNCTION__);
		command_stat = -ENOMEM;
		goto error;
	}
	
	memcpy(response->response_struct.read_resp.data.data_val, buffer2,
			MAX_EXTENT_SIZE);

	//TODO:Write back the erasure coded data 
	return response;

error:

	return NULL;
}

sstack_payload_t *sstack_write(sstack_payload_t *payload,
		sfsd_t *sfsd, log_ctx_t *ctx)
{
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented", __FUNCTION__);
	return NULL;
}

sstack_payload_t *sstack_create(sstack_payload_t *payload,
		sfsd_t *sfsd, log_ctx_t *ctx)
{
	/* Locate an extent */
	return NULL;
}

sstack_payload_t *sstack_mkdir(sstack_payload_t *payload, log_ctx_t *ctx)
{
	int32_t command_stat = 0;
	sstack_file_handle_t *dir_path = &payload->command_struct.extent_handle;
	sstack_payload_t *response = NULL;
	struct sstack_nfs_mkdir_cmd *mkdir_cmd =
		&payload->command_struct.mkdir_cmd;
	char *p = dir_path->name;
	p[dir_path->name_len] = '\0';

	response = bds_cache_alloc(sfsd_global_cache_arr[PAYLOAD_CACHE_OFFSET]);
	if (!response) {
		sfs_log(ctx, SFS_ERR, "%s(): Memory allocation failed",
				__FUNCTION__);
		command_stat = -ENOMEM;
		goto error;
	}
	command_stat = mkdir(p, mkdir_cmd->mode);
	sfs_log(ctx, SFS_DEBUG, "%s(): command status: %d", __FUNCTION__,
			command_stat);
	if (command_stat != 0)
		goto error;

	/* Command is successful */
	response->hdr.sequence = payload->hdr.sequence;
	response->hdr.payload_len = sizeof(*response);
	response->response_struct.command_ok = command_stat;

	return response;

error:
	payload->response_struct.command_ok = command_stat;
	if (response)
		bds_cache_free(sfsd_global_cache_arr[PAYLOAD_CACHE_OFFSET],
				response);
	return payload;

}

sstack_payload_t *sstack_symlink(sstack_payload_t *payload, log_ctx_t *ctx)
{
	int32_t command_stat = 0;
	sstack_file_handle_t *old_path = &payload->command_struct.extent_handle;
	sstack_file_handle_t *new_path =
		&payload->command_struct.symlink_cmd.new_path;
	sstack_payload_t *response = payload;

	char *p = old_path->name;
	char *q = new_path->name;

	p[old_path->name_len - 1] = '\0';
	q[new_path->name_len - 1] = '\0';

	command_stat = symlink(p, q);
	sfs_log(ctx, SFS_DEBUG, "%s(): command status: %d", __FUNCTION__,
			command_stat);

	if (command_stat != 0)
		response->response_struct.command_ok = errno;

	return response;
}

sstack_payload_t *sstack_mknod(sstack_payload_t *payload, log_ctx_t *ctx)
{
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented", __FUNCTION__);
	return NULL;
}

sstack_payload_t *sstack_remove( sstack_payload_t *payload, log_ctx_t *ctx)
{
	sstack_file_handle_t *file_handle =
		&payload->command_struct.extent_handle;
	sstack_payload_t *response = payload;
	char *p = file_handle->name;
	int32_t command_stat = 0;

	p[file_handle->name_len - 1] = '\0';

	command_stat = remove(p);

	if (command_stat != 0)
		response->response_struct.command_ok = errno;

	return response;
}

sstack_payload_t *sstack_rmdir(sstack_payload_t *payload, log_ctx_t *ctx)
{
	int32_t command_stat = 0;
	sstack_payload_t *response = payload;
	sstack_file_handle_t *file_handle = &payload->command_struct.extent_handle;
	char *p = file_handle->name;

	p[file_handle->name_len - 1] = '\0';

	command_stat = rmdir(p);
	sfs_log(ctx, SFS_DEBUG, "%s(): command status: %d", __FUNCTION__,
			command_stat);

	if (command_stat != 0)
		response->response_struct.command_ok = errno;

	return response;
}

sstack_payload_t *sstack_rename(sstack_payload_t *payload, log_ctx_t *ctx)
{
	int32_t command_stat = 0;
	sstack_payload_t *response = payload;
	sstack_file_handle_t *old_path = &payload->command_struct.extent_handle;
	sstack_file_handle_t *new_path =
		&payload->command_struct.rename_cmd.new_path;
	char *p = old_path->name;
	char *q = new_path->name;

	p[old_path->name_len - 1] = '\0';
	q[new_path->name_len - 1] = '\0';

	command_stat = rename(p, q);

	if (command_stat != 0)
		response->response_struct.command_ok = errno;

	return response;
}

sstack_payload_t *sstack_link(sstack_payload_t *payload, log_ctx_t *ctx)
{
	int32_t command_stat = 0;
	sstack_file_handle_t *old_path = &payload->command_struct.extent_handle;
	sstack_file_handle_t *new_path =
		&payload->command_struct.symlink_cmd.new_path;
	sstack_payload_t *response = payload;

	char *p = old_path->name;
	char *q = new_path->name;

	p[old_path->name_len - 1] = '\0';
	q[new_path->name_len - 1] = '\0';

	command_stat = link(p, q);
	sfs_log(ctx, SFS_DEBUG, "%s(): command status: %d", __FUNCTION__,
			command_stat);

	if (command_stat != 0)
		response->response_struct.command_ok = errno;

	return response;
}

sstack_payload_t *sstack_readdir(sstack_payload_t *payload, log_ctx_t *ctx)
{
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented", __FUNCTION__);
	return NULL;
}

sstack_payload_t *sstack_readdirplus(sstack_payload_t *payload, log_ctx_t *ctx)
{
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented", __FUNCTION__);
	return NULL;
}

sstack_payload_t *sstack_fsstat(sstack_payload_t *payload, log_ctx_t *ctx)
{
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented", __FUNCTION__);
	return NULL;
}

sstack_payload_t *sstack_fsinfo(sstack_payload_t *payload, log_ctx_t *ctx)
{
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented", __FUNCTION__);
	return NULL;
}

sstack_payload_t *sstack_pathconf(sstack_payload_t *payload, log_ctx_t *ctx)
{
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented", __FUNCTION__);
	return NULL;
}

sstack_payload_t *sstack_commit(sstack_payload_t *payload, log_ctx_t *ctx)
{
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented", __FUNCTION__);
	return NULL;
}
