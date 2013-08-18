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

sstack_payload_t* sstack_getattr(
		sstack_payload_t *payload,
		bds_cache_desc_t payload_data_cache[2],
		log_ctx_t *ctx)
{

	int32_t command_stat = 0;
	sstack_file_name_t *file_handle = &payload->command_struct.file_handle;
	char *p = file_handle->name;
	sstack_payload_t *response = NULL;
	struct sstack_nfs_getattr_resp *getattr_resp = NULL;
	
	/* Make the string null terminated */
	p[file_handle->name_len - 1] = 0; 
	sfs_log(ctx, SFS_DEBUG, ":%s extent path:%s", __FUNCTION__, p);

	//i= find_rorw_branch(path); - Might need later
	response = bds_cache_alloc(payload_data_cache[PAYLOAD_CACHE_OFFSET]);
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
		bds_cache_free(payload_data_cache[PAYLOAD_CACHE_OFFSET],
				response);
	return payload;
}


sstack_payload_t *sstack_setattr(
		sstack_payload_t *payload,
		bds_cache_desc_t payload_data_cache[2],
		log_ctx_t *ctx)
{
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented", __FUNCTION__);
	return NULL;
}

sstack_payload_t *sstack_lookup(
		sstack_payload_t *payload,
		bds_cache_desc_t payload_data_cache[2],
		log_ctx_t *ctx)
{
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented", __FUNCTION__);
	return NULL;
}

sstack_payload_t *sstack_access(
		sstack_payload_t *payload,
		bds_cache_desc_t payload_data_cache[2],
		log_ctx_t *ctx)
{
	int command_stat = 0;
	struct sstack_nfs_access_cmd *cmd = &payload->command_struct.access_cmd;
	sstack_file_name_t *file_handle = &payload->command_struct.file_handle;
	sstack_payload_t *response = NULL;
	struct sstack_nfs_access_resp *access_resp;
	char *p = file_handle->name;
	p[file_handle->name_len - 1] = '\0';
	sfs_log(ctx, SFS_DEBUG, "%s(): path", __FUNCTION__, p);

	response = bds_cache_alloc(payload_data_cache[PAYLOAD_CACHE_OFFSET]);
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
		bds_cache_free(payload_data_cache[PAYLOAD_CACHE_OFFSET],
				response);
	return payload;
}


sstack_payload_t *sstack_readlink(
		sstack_payload_t *payload,
		bds_cache_desc_t payload_data_cache[2],
		log_ctx_t *ctx)
{
	int32_t command_stat = 0;
	sstack_file_name_t *link = &payload->command_struct.file_handle;
	sstack_payload_t *response = NULL;
	struct sstack_nfs_readlink_resp *readlink_resp = NULL;
	char *p = link->name;
	p[link->name_len - 1] = '\0';

	response = bds_cache_alloc(payload_data_cache[PAYLOAD_CACHE_OFFSET]);
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
		bds_cache_free(payload_data_cache[PAYLOAD_CACHE_OFFSET],
				response);
	return payload;

}

sstack_payload_t *sstack_read(
		sstack_payload_t *payload,
		bds_cache_desc_t payload_data_cache[2],
		log_ctx_t *ctx)
{
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented", __FUNCTION__);
	return NULL;
}

sstack_payload_t *sstack_write(
		sstack_payload_t *payload,
		bds_cache_desc_t payload_data_cache[2],
		log_ctx_t *ctx)
{
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented", __FUNCTION__);
	return NULL;
}

sstack_payload_t *sstack_create(
		sstack_payload_t *payload,
		bds_cache_desc_t payload_data_cache[2],
		log_ctx_t *ctx)
{
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented", __FUNCTION__);
	return NULL;
}

sstack_payload_t *sstack_mkdir(
		sstack_payload_t *payload,
		bds_cache_desc_t payload_data_cache[2],
		log_ctx_t *ctx)
{
	int32_t command_stat = 0;
	sstack_file_name_t *dir_path = &payload->command_struct.file_handle;
	sstack_payload_t *response = NULL;
	struct sstack_nfs_mkdir_cmd *mkdir_cmd =
		&payload->command_struct.mkdir_cmd;
	char *p = dir_path->name;
	p[dir_path->name_len] = '\0';
	
	response = bds_cache_alloc(payload_data_cache[PAYLOAD_CACHE_OFFSET]);
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
		bds_cache_free(payload_data_cache[PAYLOAD_CACHE_OFFSET],
				response);
	return payload;

}

sstack_payload_t *sstack_symlink(
		sstack_payload_t *payload,
		bds_cache_desc_t payload_data_cache[2],
		log_ctx_t *ctx)
{
	int32_t command_stat = 0;
	sstack_file_name_t *old_path = &payload->command_struct.file_handle;
	sstack_file_name_t *new_path =
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

sstack_payload_t *sstack_mknod(
		sstack_payload_t *payload,
		bds_cache_desc_t payload_data_cache[2],
		log_ctx_t *ctx)
{
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented", __FUNCTION__);
	return NULL;
}

sstack_payload_t *sstack_remove(
		sstack_payload_t *payload,
		bds_cache_desc_t payload_data_cache[2],
		log_ctx_t *ctx)
{
	sstack_file_name_t *file_handle = &payload->command_struct.file_handle;
	sstack_payload_t *response = payload;
	char *p = file_handle->name;
	int32_t command_stat = 0;

	p[file_handle->name_len - 1] = '\0';

	command_stat = remove(p);

	if (command_stat != 0)
		response->response_struct.command_ok = errno;

	return response;
}

sstack_payload_t *sstack_rmdir(
		sstack_payload_t *payload,
		bds_cache_desc_t payload_data_cache[2],
		log_ctx_t *ctx)
{
	int32_t command_stat = 0;
	sstack_payload_t *response = payload;
	sstack_file_name_t *file_handle = &payload->command_struct.file_handle;
	char *p = file_handle->name;

	p[file_handle->name_len - 1] = '\0';

	command_stat = rmdir(p);
	sfs_log(ctx, SFS_DEBUG, "%s(): command status: %d", __FUNCTION__,
			command_stat);

	if (command_stat != 0)
		response->response_struct.command_ok = errno;

	return response;
}

sstack_payload_t *sstack_rename(
		sstack_payload_t *payload,
		bds_cache_desc_t payload_data_cache[2],
		log_ctx_t *ctx)
{
	int32_t command_stat = 0;
	sstack_payload_t *response = payload;
	sstack_file_name_t *old_path = &payload->command_struct.file_handle;
	sstack_file_name_t *new_path =
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

sstack_payload_t *sstack_link(
		sstack_payload_t *payload,
		bds_cache_desc_t payload_data_cache[2],
		log_ctx_t *ctx)
{
	int32_t command_stat = 0;
	sstack_file_name_t *old_path = &payload->command_struct.file_handle;
	sstack_file_name_t *new_path =
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

sstack_payload_t *sstack_readdir(
		sstack_payload_t *payload,
		bds_cache_desc_t payload_data_cache[2],
		log_ctx_t *ctx)
{
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented", __FUNCTION__);
	return NULL;
}

sstack_payload_t *sstack_readdirplus(
		sstack_payload_t *payload,
		bds_cache_desc_t payload_data_cache[2],
		log_ctx_t *ctx)
{
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented", __FUNCTION__);
	return NULL;
}

sstack_payload_t *sstack_fsstat(
		sstack_payload_t *payload,
		bds_cache_desc_t payload_data_cache[2],
		log_ctx_t *ctx)
{
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented", __FUNCTION__);
	return NULL;
}

sstack_payload_t *sstack_fsinfo(
		sstack_payload_t *payload,
		bds_cache_desc_t payload_data_cache[2],
		log_ctx_t *ctx)
{
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented", __FUNCTION__);
	return NULL;
}

sstack_payload_t *sstack_pathconf(
		sstack_payload_t *payload,
		bds_cache_desc_t payload_data_cache[2],
		log_ctx_t *ctx)
{
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented", __FUNCTION__);
	return NULL;
}

sstack_payload_t *sstack_commit(
		sstack_payload_t *payload,
		bds_cache_desc_t payload_data_cache[2],
		log_ctx_t *ctx)
{
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented", __FUNCTION__);
	return NULL;
}

