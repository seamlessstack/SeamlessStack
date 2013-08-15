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
#include <sstack_nfs.h>
#include <sstack_jobs.h>
#include <sstack_chunk.h>
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
	sfs_log(ctx, SFS_DEBUG, "%s", p);

	//i= find_rorw_branch(path); - Might need later
	response = bds_cache_alloc(payload_data_cache[0]);
	if (!response) {
		command_stat = -ENOMEM;
		goto error;
	}
	
	getattr_resp = &response->response_struct.getattr_resp;
	command_stat = lstat(p, &getattr_resp->stbuf);
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

	/* Command is successfull */

	response->hdr.sequence = payload->hdr.sequence;
	response->hdr.payload_len = sizeof(*response);
	response->response_struct.command_ok = command_stat;
	
	return response;

error:
	payload->response_struct.command_ok = command_stat;
	if (response)
		bds_cache_free(payload_data_cache[0], response);
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
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented", __FUNCTION__);
	return NULL;
}


sstack_payload_t *sstack_readlink(
		sstack_payload_t *payload,
		bds_cache_desc_t payload_data_cache[2],
		log_ctx_t *ctx)
{
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented", __FUNCTION__);
	return NULL;
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
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented", __FUNCTION__);
	return NULL;
}

sstack_payload_t *sstack_symlink(
		sstack_payload_t *payload,
		bds_cache_desc_t payload_data_cache[2],
		log_ctx_t *ctx)
{
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented", __FUNCTION__);
	return NULL;
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
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented", __FUNCTION__);
	return NULL;
}

sstack_payload_t *sstack_rmdir(
		sstack_payload_t *payload,
		bds_cache_desc_t payload_data_cache[2],
		log_ctx_t *ctx)
{
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented", __FUNCTION__);
	return NULL;
}

sstack_payload_t *sstack_rename(
		sstack_payload_t *payload,
		bds_cache_desc_t payload_data_cache[2],
		log_ctx_t *ctx)
{
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented", __FUNCTION__);
	return NULL;
}

sstack_payload_t *sstack_link(
		sstack_payload_t *payload,
		bds_cache_desc_t payload_data_cache[2],
		log_ctx_t *ctx)
{
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented", __FUNCTION__);
	return NULL;
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

