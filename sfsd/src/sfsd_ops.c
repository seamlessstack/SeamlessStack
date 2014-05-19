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
#include <policy.h>

#define USE_INDEX 0
#define USE_HANDLE 1
#define ERASURE_STRIPE_SIZE (64*1024)
#define INVALID_INDEX -1

/* Compilation error fix */
#define PAYLOAD_CACHE_OFFSET 0
#define DATA64K_CACHE_OFFSET 1
#define DATA4K_CACHE_OFFSET 2

extern bds_cache_desc_t sfsd_global_cache_arr[];
extern char *get_mount_path(sfs_chunk_domain_t * ,
							sstack_file_handle_t * , char ** );
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
	sfs_log(ctx, SFS_DEBUG, ":%s extent path:%s\n", __FUNCTION__, p);

	//i= find_rorw_branch(path); - Might need later
	response = sstack_create_payload(NFS_GETATTR_RSP);
	if (!response) {
		command_stat = -ENOMEM;
		goto error;
	}

	getattr_resp = &response->response_struct.getattr_resp;
	command_stat = lstat(p, &getattr_resp->stbuf);
	sfs_log(ctx, SFS_DEBUG, "%s(): command status: %d\n", __FUNCTION__,
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
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented\n", __FUNCTION__);
	return payload;
}

sstack_payload_t *sstack_lookup(sstack_payload_t *payload, log_ctx_t *ctx)
{
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented\n", __FUNCTION__);
	return payload;
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
	sfs_log(ctx, SFS_DEBUG, "%s(): path\n", __FUNCTION__, p);

	response = sstack_create_payload(NFS_ACCESS_RSP);
	if (!response) {
		sfs_log(ctx, SFS_ERR, "%s(): Memory allocation failed\n",
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
	sfs_log(ctx, SFS_DEBUG, "%s(): command status: %d\n", __FUNCTION__,
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

	response = sstack_create_payload(NFS_READLINK_RSP);
	if (!response) {
		sfs_log(ctx, SFS_ERR, "%s(): Memory allocation failed\n",
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
	sfs_log(ctx, SFS_DEBUG, "%s(): command status: %d\n", __FUNCTION__,
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

/*
 * @extents - Array of extents to be searched from
 * @index - the index of the extent to be used. if this is < 0
 *    then the extent should be searched using extent handle
 * @extent_handle - the extent key (file handle to be searched)
 * @sfsd - the sfsd structure on which the operation is taking place
 * @mounted_path - the path of the extent w.r.t the mount point in
 *   the sfsd. This is an output from the function.
 *
 * @return: the index of the extent if found in the array else
 *   appropriate error code.
 */
static size_t get_extent(sstack_extent_t *extents, size_t num_extents,
						 ssize_t index, sstack_file_handle_t *extent_handle,
						 sfsd_t *sfsd, char *mounted_path, log_ctx_t *ctx)
{
	int32_t i;
	char *p = extent_handle->name;
	char *q = NULL;
	char *r;
	size_t ret = -1, len = 0;

	sstack_extent_t *extent = NULL;

	if (index > num_extents) {
		sfs_log(ctx, SFS_ERR, "%s()-passed index greater than size of array\n",
				__FUNCTION__);
		goto ret;
	}

	if (index >= 0) {
		extent = &extents[index];
		ret = index;
	} else {
		for(i = 0; i < num_extents; ++i) {
			if (TRUE == match_extent(extent_handle,
									 extents[i].e_path)) {
				extent = &extents[i];
				ret = i;
				break;
			}
		}
	}

	if (extent == NULL) {
		sfs_log(ctx, SFS_ERR, "%s(): Error getting extent\n");
		ret = -EINVAL;
		goto ret;
	}

	if (mounted_path != NULL) {
		if ((q = get_mount_path(sfsd->chunk, extent_handle, &r)) == NULL) {
			sfs_log(ctx, SFS_ERR, "%s(): Invalid mount path for:%s\n", p);
			ret = -EINVAL;
			goto ret;
		}
		/* Create the file path relative to the mount point */
		len = strlen(r); /* r contains the nfs export */
		p += len; /* p points to start of the path minus the nfs export */

		/* Create the file path relative to the mount point */
		len = strlen(r); /* r contains the nfs export */
		p += len; /* p points to start of the path minus the nfs export */
		sprintf (mounted_path, "%s%s",q, p);
	}

ret:
	return ret;
}

/* Return the number of bytes read from disk if successful else
 *  returns < 0
 */
static int32_t read_check_extent(char *mounted_path, size_t read_size,
								 int32_t check_cksum, uint32_t cksum,
								 void *buffer, log_ctx_t *ctx)
{
	int32_t ret = 0;
	int32_t fd = -1;
	ssize_t nbytes;
	/* Parameter Validation */
	if (mounted_path == NULL || buffer == NULL) {
		sfs_log(ctx, SFS_ERR, "%s() - Invalid Parameters\n", __FUNCTION__);
		goto ret;
	}
	if ((fd = open(mounted_path, O_RDONLY|O_DIRECT)) < 0) {
		sfs_log(ctx, SFS_ERR, "%s(): extent '%s' open failed\n",
				__FUNCTION__, mounted_path);
		ret = errno;
		goto ret;
	}
	nbytes = read(fd, buffer, read_size);
	if (nbytes < 0) {
		sfs_log(ctx, SFS_ERR, "%s(): Read failure\n",
			__FUNCTION__);
		ret = errno;
		goto ret;
	}
	ret = nbytes;
	if (check_cksum) {
		uint32_t crc;
		crc = crc_pcl((const char *)buffer, nbytes, 0);
		if (crc != cksum) {
			ret = -SSTACK_ECKSUM;
		}
	}
ret:
	return ret;
}


static int32_t write_update_extent(char *mounted_path, size_t write_size,
								   int32_t calculate_cksum, uint32_t *cksum,
								   void *buffer, log_ctx_t *ctx)
{
	int32_t ret = 0;
	int32_t fd = -1;
	ssize_t nbytes;
	/* Parameter Validation */
	sfs_log(ctx, SFS_DEBUG, "mounted path: %s, write_size: %d, buffer: %p"
			"cksum: %p, calculate_cksum: %d\n", mounted_path, write_size,
			buffer, calculate_cksum, cksum);
	if (mounted_path == NULL || buffer == NULL
		||(calculate_cksum && (cksum == NULL))) {
		sfs_log(ctx, SFS_ERR, "%s() - Invalid Parameters\n", __FUNCTION__);
		goto ret;
	}
	if ((fd = open(mounted_path, O_WRONLY)) < 0) {
		sfs_log(ctx, SFS_ERR, "%s(): extent '%s' open failed\n",
				__FUNCTION__, mounted_path);
		ret = errno;
		goto ret;
	}
	nbytes = write(fd, buffer, write_size);
	if (nbytes < 0) {
		sfs_log(ctx, SFS_ERR, "%s(): write failure error: %d\n",
			__FUNCTION__, errno);
		ret = errno;
		goto ret;
	} else {
		sfs_log(ctx, SFS_DEBUG, "Wrote %d bytes for extent %s\n",
				nbytes, mounted_path);
	}
	ret = nbytes;
	if (calculate_cksum) {
		*cksum = crc_pcl((const char *)buffer, nbytes, 0);
		sfs_log(ctx, SFS_DEBUG, "calulated checksum for extent %s : %d\n",
				mounted_path, *cksum);
	}
ret:
	return ret;
}

static ssize_t read_erasure_code(void *d_stripes[], void *c_stripes[],
								int32_t err_strps[], size_t index,
								sstack_inode_t *inode,sfsd_t *sfsd,
								log_ctx_t *ctx)
{
	sstack_extent_t *extent = NULL;
	size_t extent_index;
	char *path;
	ssize_t ret;
	ssize_t er_index, ecode_idx, final_dstripe_idx, i, j;
	ssize_t num_err_strps, num_dstripes, erasure_index;


	path = bds_cache_alloc(sfsd_global_cache_arr[DATA4K_CACHE_OFFSET]);
	if (path == NULL) {
		sfs_log(ctx, SFS_ERR, "%s() - No memory for path\n",
				__FUNCTION__);
		ret = -ENOMEM;
		goto ret;
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
			for(j = i; j >= index; j--)
				bds_cache_free(sfsd_global_cache_arr[DATA64K_CACHE_OFFSET],
							   d_stripes[j]);
			ret = -ENOMEM;
			goto ret;
		}
		extent_index = get_extent(inode->i_extent, inode->i_numextents,
								  i, NULL, sfsd, path, ctx);
		if (extent_index > 0) {
			ret = read_check_extent(path, ERASURE_STRIPE_SIZE, TRUE,
									inode->i_extent[i].e_cksum,
									d_stripes[i-index], ctx);
			if (ret == -SSTACK_ECKSUM) {
				err_strps[num_err_strps++] = (i-index);
			}
			num_dstripes++;
		}
	}

	for (i = er_index; i < er_index + CODE_STRIPES; i++) {
		c_stripes[i-er_index] =
			bds_cache_alloc(sfsd_global_cache_arr[DATA64K_CACHE_OFFSET]);
			if (c_stripes[i-er_index] == NULL) {
				sfs_log(ctx, SFS_ERR, "%s(): Error getting read buffer\n",
						__FUNCTION__);
				ret = -ENOMEM;
				for(j = i; j >= er_index; j--)
					bds_cache_free(sfsd_global_cache_arr[DATA64K_CACHE_OFFSET],
								   c_stripes[j]);
					ret = -ENOMEM;
					goto ret;
			}

			erasure_index = get_extent(inode->i_erasure, inode->i_numerasure,
									   i, NULL, sfsd, path, ctx);
			if (erasure_index > 0) {
				ret = read_check_extent(path, MAX_EXTENT_SIZE, TRUE,
										inode->i_erasure[i].e_cksum,
										c_stripes[i-er_index], ctx);
				if (ret == -SSTACK_ECKSUM) {
					err_strps[num_err_strps++] = (i-er_index);
				}
			}
	}

	ret = sfs_esure_decode(d_stripes, num_dstripes, c_stripes, CODE_STRIPES,
						   err_strps, num_err_strps, ERASURE_STRIPE_SIZE);

	/* Data is placed at *index* */
	if (ret == ESURE_SUCCESS)
		ret = index;
	else
		ret = ESURE_ERR;
ret:
	return ret;
}

static ssize_t write_erasure_code(void *d_stripes[], void *c_stripes[],
								  int32_t err_strps[], size_t index,
								  sstack_inode_t *inode,sfsd_t *sfsd,
								  log_ctx_t *ctx)
{
	return 0;
}

static ssize_t deapply_policies(struct policy_entry *pe, void *in_buf,
									  size_t in_size, void **out_buf)
{
	void *run_buf = in_buf;
	void *policy_buf = NULL;
	size_t out_size = 0;
	int i = 0;

	for(i = pe->pe_num_plugins - 1; i >= 0; i--) {
		struct plugin_entry_points *entry =
			get_plugin_entry_points(pe->pe_policy[i]->pp_policy_name);
		if (entry) {
			if ((out_size = entry->remove(run_buf, &policy_buf, in_size)) > 0) {
				if (bds_cache_free(sfsd_global_cache_arr[DATA64K_CACHE_OFFSET],
							run_buf) < 0) {
					/* Memory allocated by malloc/realloc */
					free(run_buf);
				}
				run_buf = policy_buf;
				in_size = out_size;
			}
		}
	}
	*out_buf = policy_buf;
	return out_size;
}

static ssize_t apply_policies(struct policy_entry *pe, void *in_buf,
							  ssize_t in_size, void **out_buf)
{
	void *run_buf = in_buf;
	void *policy_buf = NULL;
	size_t out_size = 0;
	int i = 0;
	for(i = 0; i < pe->pe_num_plugins; i++) {
		struct plugin_entry_points *entry =
			get_plugin_entry_points(pe->pe_policy[i]->pp_policy_name);
		if (entry) {
			if ((out_size = entry->apply(run_buf, &policy_buf, in_size)) > 0) {
				if (bds_cache_free(sfsd_global_cache_arr[DATA64K_CACHE_OFFSET],
							run_buf) < 0) {
					/* Memory allocated by malloc/realloc */
					free(run_buf);
				}
				run_buf = policy_buf;
				in_size = out_size;
			}
		}
	}
	*out_buf = policy_buf;
	return out_size;
}

sstack_payload_t *sstack_read(sstack_payload_t *payload,
							  sfsd_t *sfsd, log_ctx_t *ctx)
{
	int32_t i, command_stat = 0, index = 0;
	size_t in_size, out_size;
	sstack_inode_t *inode;
	struct sstack_nfs_read_cmd *cmd = &payload->command_struct.read_cmd;
	struct policy_entry *pe = &cmd->pe;
	void *d_stripes[MAX_DATA_STRIPES], *c_stripes[CODE_STRIPES];
	int32_t err_strps[MAX_DATA_STRIPES + CODE_STRIPES];
	void *buffer, *policy_buf = NULL, *run_buf = NULL, *out_buf = NULL;
	char mount_path[PATH_MAX];
	ssize_t extent_index;

	inode = bds_cache_alloc(sfsd_global_cache_arr[INODE_CACHE_OFFSET]);
	if (inode == NULL) {
		command_stat = -ENOMEM;
		sfs_log(ctx, SFS_ERR, "%s(): %s\n",
			__FUNCTION__, "Inode cache mem not available");
		goto error;
	}

	if (get_inode(cmd->inode_no, inode, sfsd->db) != 0) {
		command_stat = -EINVAL;
		sfs_log(ctx, SFS_ERR, "%s(): Inode not available\n",
			__FUNCTION__);
		goto error;
	}

	/* Get the extent to read */
	extent_index = get_extent(inode->i_extent, inode->i_numextents,
							  INVALID_INDEX,
							  &payload->command_struct.extent_handle,
							  sfsd, mount_path, ctx);
	if (extent_index < 0) {
		sfs_log(ctx, SFS_ERR, "%s() - Invalid extent to read\n", __FUNCTION__);
		command_stat = -EINVAL;
		goto error;
	}

	/* Read the erasure coded stripes since we are asked to
	   do so :-)
	*/
	if (cmd->read_ecode) {
		index = read_erasure_code(d_stripes, c_stripes, err_strps,
								  extent_index, inode, sfsd, ctx);
		if (index < 0) {
			sfs_log(ctx, SFS_ERR, "%s() - Erasure code gone bad\n",
							__FUNCTION__);
			command_stat = index;
			goto error;
		} else {
		/* Data is placed at *index* */
			buffer = d_stripes[index];
		}
	} else {
		/* We are not required to read erasure code.
		 * Just try to do a normal read
		 * and report success or failure
		 */
		buffer = bds_cache_alloc(sfsd_global_cache_arr[DATA64K_CACHE_OFFSET]);
		if (buffer == NULL) {
			sfs_log(ctx, SFS_ERR, "%s(): Error getting read buffer\n",
					__FUNCTION__);
			command_stat = -ENOMEM;
			goto error;
		}
		/* Read the actual extent now */
		command_stat = read_check_extent(mount_path, MAX_EXTENT_SIZE, TRUE,
										 inode->i_extent[extent_index].e_cksum,
										 buffer, ctx);
		if (command_stat < 0) {
			sfs_log(ctx, SFS_ERR, "%s(): Error reading extent\n",
					__FUNCTION__);
			command_stat = -ENODATA;
			goto error;
		}
	}

	/* By this time we have some valid data with us in *buffer -
	 *  Need to de-apply policies on it
	 */

	out_size = deapply_policies(pe, buffer, MAX_EXTENT_SIZE, &policy_buf);

	/*
	 * Modify the sent payload to make it a response.
	 * Save an allocation here
	 */
	payload->response_struct.command_ok = SSTACK_SUCCESS;
	payload->command = NFS_READ_RSP;
	payload->response_struct.read_resp.count = out_size;
	payload->response_struct.read_resp.eof = 0;
	payload->response_struct.read_resp.data.data_len = out_size;
	payload->response_struct.read_resp.data.data_buf = policy_buf;
	//TODO:Write back the erasure coded data - write_erasure_code();
	return payload;

error:
	payload->response_struct.command_ok = command_stat;
	payload->command = NFS_READ_RSP;
	return payload;
}

sstack_payload_t *sstack_write(sstack_payload_t *payload,
							   sfsd_t *sfsd, log_ctx_t *ctx)
{
	int32_t i, command_stat = 0;
	sstack_inode_t *inode;
	struct sstack_nfs_write_cmd *cmd = &payload->command_struct.write_cmd;
	struct sstack_file_handle_t *extent_handle =
		&payload->command_struct.extent_handle;
	struct policy_entry *pe = &cmd->pe;
	void *buffer = NULL, *run_buf = NULL, *policy_buf = NULL;
	size_t in_size, out_size;
	int chunk_index = -1;
	char *mount_path = NULL;
	char extent_name[PATH_MAX];
	ssize_t extent_index = -1, erasure_index = -1;
	int fd;
	char *policy_inbuf, *policy_outbuf;
	uint32_t cksum = 0;
	sstack_extent_t *new_extents = NULL;
	uint32_t extent_created = 0;
	unsigned long long size = 0;

	sfs_log(ctx, SFS_DEBUG, "%s() - %d\n",
			__FUNCTION__, __LINE__);
	inode = bds_cache_alloc(sfsd->local_caches[INODE_CACHE_OFFSET]);
	sfs_log(ctx, SFS_DEBUG, "%s() - %d %p\n", __FUNCTION__, __LINE__, inode);

	if (inode == NULL) {
		command_stat = -ENOMEM;
		sfs_log(ctx, SFS_ERR, "%s(): %s\n",
			__FUNCTION__, "Inode cache mem not available\n");
		goto error;
	}
	sfs_log(ctx, SFS_DEBUG, "%s() - %d\n",
			__FUNCTION__, __LINE__);

	if (get_inode(cmd->inode_no, inode, sfsd->db) != 0) {
		command_stat = -EINVAL;
		sfs_log(ctx, SFS_ERR, "%s(): Inode not available\n",
			__FUNCTION__);
		goto error;
	}
	sfs_log(ctx, SFS_DEBUG, "%s() - %d\n",
			__FUNCTION__, __LINE__);

	/* if the payload handle is NULL, it means this is a request for
	 * a new extent.
	 */
	if (payload->command_struct.extent_handle.name_len == 0) {
		sfs_log(ctx, SFS_DEBUG, "%s() - Came in to create new extent\n",
				__FUNCTION__);
		chunk_index = sfsd->chunk->schedule(sfsd->chunk);
		if (chunk_index >= 0) {
			mount_path = sfsd->chunk->storage[chunk_index].mount_point;
			sprintf(extent_name, "%s/%u-XXXXXX", mount_path, time(NULL));
			sfs_log(ctx, SFS_DEBUG, "extent name: %s data len %d\n",
					extent_name, cmd->data.data_len);
			fd = open(extent_name, O_CREAT|O_WRONLY,
					S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
			sfs_log(ctx, SFS_DEBUG, "fd: %d, error: %d\n", fd, errno);
			if (fd < 0) {
				sfs_log(ctx, SFS_ERR, "%s() - Unable to create a new extent\n",
						__FUNCTION__);
				command_stat = errno;
				goto error;
			}
			extent_created = 1;
			/* A new extent is created */
			close(fd);
		}
	} else {
		/* Get the extent to write to */
		extent_index = get_extent(inode->i_extent, inode->i_numextents,
								  INVALID_INDEX,
								  extent_name, sfsd, mount_path, ctx);
		if (extent_index < 0) {
			sfs_log(ctx, SFS_ERR, "%s() -  Invalid extent to write\n",
					__FUNCTION__);
			command_stat = -EINVAL;
			goto error;
		}
	}
	sfs_log(ctx, SFS_DEBUG, "%s() - %d\n",
			__FUNCTION__, __LINE__);

	if (cmd->offset == 0) {
		/* Go ahead -
		   1) Apply policy
		   2) out_size would have the no. of bytes to write to disk.
		*/
		sfs_log(ctx, SFS_DEBUG, "pe_num_plugins: %d\n", pe->pe_num_plugins);
		if (pe->pe_num_plugins) {
			out_size = apply_policies(pe, buffer, cmd->count, &policy_outbuf);
		} else {
			out_size = cmd->data.data_len;
			policy_outbuf = cmd->data.data_buf;
		}
	} else {
		out_size = read_check_extent(mount_path, MAX_EXTENT_SIZE, TRUE,
										 inode->i_extent[extent_index].e_cksum,
										 buffer, ctx);
		if (out_size < 0) {
			command_stat = out_size;
			goto error;
		}
		out_size = deapply_policies(pe, buffer, out_size, &policy_inbuf);
		memcpy(policy_inbuf[cmd->offset], cmd->data.data_buf, cmd->count);
		if (pe->pe_num_plugins) {
			out_size = apply_policies(pe, policy_inbuf,
					out_size, &policy_outbuf);
		} else {
			out_size = cmd->data.data_len;
			policy_outbuf = cmd->data.data_buf;
		}
	}

	command_stat = write_update_extent(extent_name, out_size, TRUE, &cksum,
									   policy_outbuf, ctx);
	sfs_log(ctx, SFS_DEBUG, "command status: %d\n", command_stat);
	if (command_stat < 0)
		goto error;

	/* Now go ahead update the metadata */

	if (extent_created) {
		sstack_extent_t *extent;
		sfs_log(ctx, SFS_DEBUG,
				"%s() - A new extent has been created,update extent\n",
				__FUNCTION__);
		pthread_mutex_lock(&inode->i_lock);
		new_extents = realloc(inode->i_extent, sizeof(*(inode->i_extent))
							  * (inode->i_numextents + 1));
		if (new_extents)
			inode->i_extent = new_extents;
		else
			goto error;

		sfs_log(ctx, SFS_DEBUG, "%s() - A new extent structure has been allocated\n",
				__FUNCTION__);
		extent = &inode->i_extent[inode->i_numextents];
		memset(extent, 0, sizeof(sstack_extent_t));
		extent->e_size = cmd->data.data_len;
		extent->e_sizeondisk = out_size;
		extent->e_cksum = cksum;
		extent->e_path = malloc(sizeof(sstack_file_handle_t));
		sprintf (extent->e_path->name, "%s/%s",
				 sfsd->chunk->storage[chunk_index].path,
				 basename(extent_name));
		sfs_log(ctx, SFS_DEBUG, "%s() - handle name: %s\n",
				__FUNCTION__, extent->e_path->name);
		extent->e_path->name_len = strlen(extent_name);
		if (inode->i_size != 0) {
			sstack_extent_t *prev_extent;
			prev_extent = extent - 1;
			extent->e_offset = prev_extent->e_offset + prev_extent->e_size;
			extent->e_path->proto = prev_extent->e_path->proto;
			memcpy(&extent->e_path->address,&prev_extent->e_path->address,
				   sizeof(sstack_address_t));
		} else {
			sfs_log(ctx, SFS_DEBUG, "%s() == New extent path\n",
					__FUNCTION__);
			extent->e_offset = 0;
			extent->e_path->proto =
				sfsd->chunk->storage[chunk_index].protocol;
			memcpy(&extent->e_path->address,
				   &sfsd->chunk->storage[chunk_index].address,
				   sizeof(sstack_address_t));
		}
		inode->i_numextents++;
	}
	// Update inode size
	for (i = 0; i < inode->i_numextents; i++)
		size += inode->i_extent[i].e_size;

	inode->i_size = size;
	sfs_log(ctx, SFS_DEBUG, "%s: Updated size = %"PRIx64"\n",
			__FUNCTION__, inode->i_size);
	put_inode(inode, sfsd->db, 1);
	sstack_free_inode_res(inode, ctx);
	// Check
	get_inode(inode->i_num, inode, sfsd->db);
	sfs_log(ctx, SFS_DEBUG, "%s: inode name = %s number = %d size = %d\n",
			__FUNCTION__, inode->i_name, inode->i_num, inode->i_size);
	sstack_free_inode_res(inode, ctx);

	bds_cache_free(sfsd->local_caches[INODE_CACHE_OFFSET], inode);
	sfs_log(ctx, SFS_DEBUG, "%s() - put_inode, free_inode done\n",
			__FUNCTION__);

//	command_stat = write_erasure_code();

//	if (command_stat < 0)
//		goto error;
#if 0
	/* Metadata update code is here */
		pthread_mutex_lock(&inode->i_lock);
		temp = realloc(inode->i_extent, sizeof(*(inode->i_extent))
					   * (inode->i_numextents + 1));
		if (temp) {
			inode->i_extent = temp;
			extent = inode->i_extent[inode->i_numextents];
			extent->e_size = cmd->data.data_len;
			extent->e_sizeondisk = out_size;
			extent->e_cksum =
				crc_pcl((const char *)policy_buf, out_size, 0);
			sprintf (extent_handle.name, "%s/%s",
					 sfsd->chunk->storage[chunk_index].path,
					 basename(extent_name));
			extent_handle.name_len = strlen(extent_handle.name);
			if (inode->i_size) {
				prev_extent = extent - 1;
				extent->e_offset = prev_extent->e_offset + prev_extent->e_size;
				extent_handle.proto = prev_extent->e_path->proto;
				memcpy(&extent_handle.address,&prev_extent->e_path->address,
					   sizeof(sstack_address_t));
			} else {
				extent->e_offset = 0;
				extent_handle.proto =
					sfsd->chunk->storage[chunk_index].protocol;
				memcpy(&extent_handle.address,
					   &sfsd->chunk->storage[chunk_index].address,
					   sizeof(sstack_address_t));
			}
			inode->i_numextents++;
			/* Update storage */
			sfsd->chunk->storage[chunk_index].num_chunks_written++;
			sfsd->chunk->storage[chunk_index].nblocks-=
				(int32_t)ceil((1.0) * (out_size/4096));
			inode->i_size += cmd->data.data_len;
			inode->i_sizeondisk += out_size;
			put_inode(inode,sfsd->db, 1);
		}
			if (fd > 0) {
				ret =  write(fd, policy_buf, out_size);
				if (ret < out_size) {
					//TODO: Roll back the meta data update
					command_stat = errno;
					goto ret;
				}
				close(fd);
			}
		} else {
			command_stat = errno;
			goto ret;
		}
	} else {
		extent = get_extent(inode, extent_handle, sfsd);
		fd = open(extent_handle->name, O_DIRECT|O_WRONLY);
		if (fd > 0) {
			ret = write(fd, policy_buf, out_size);
			if (ret < out_size) {
				command_stat = errno;
				goto ret;
			}
		close(fd);
		}
	}

#endif
	payload->response_struct.command_ok = SSTACK_SUCCESS;
	payload->command = NFS_WRITE_RSP;
	payload->response_struct.write_resp.file_wc = command_stat;
	sfs_log(ctx, SFS_DEBUG, "%s() - Returning payload %p command stat: %d\n",
			__FUNCTION__, payload, command_stat);
	return payload;
error:
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented\n", __FUNCTION__);
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
		sfs_log(ctx, SFS_ERR, "%s(): Memory allocation failed\n",
				__FUNCTION__);
		command_stat = -ENOMEM;
		goto error;
	}
	command_stat = mkdir(p, mkdir_cmd->mode);
	sfs_log(ctx, SFS_DEBUG, "%s(): command status: %d\n", __FUNCTION__,
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

sstack_payload_t *sstack_esure_code(sstack_payload_t *payload,
										sfsd_t *sfsd, log_ctx_t *ctx)
{
	int32_t i, j, k, min_start_ext = 0, ret = 0;
	int32_t	command_stat = SSTACK_SUCCESS;
	sstack_inode_t	*inode = NULL;
	struct sstack_nfs_esure_code_cmd *cmd =
								&payload->command_struct.esure_code_cmd;
	void *d_stripes[MAX_DATA_STRIPES], *c_stripes[CODE_STRIPES];
	char mount_path[PATH_MAX];

	inode = bds_cache_alloc(sfsd_global_cache_arr[INODE_CACHE_OFFSET]);
	if (inode == NULL) {
		command_stat = -ENOMEM;
		sfs_log(ctx, SFS_ERR, "%s(): %s\n",
				__FUNCTION__, "Inode cache mem not available\n");
		goto done;
	}

	if (get_inode(cmd->inode_no, inode, sfsd->db) != 0) {
		command_stat = -EINVAL;
		sfs_log(ctx, SFS_ERR, "%s(): Inode not available\n",
				__FUNCTION__);
		goto done;
	}

	if (cmd->num_blocks == 0) {
		command_stat = SSTACK_SUCCESS;
		goto done;
	}

	/* Find the min start ext index and do erasure code from there on for
	 * all following extents.
	 * This is for both simplying as well as performance optimization as
	 * the erasure code groups for all the extents from the min start ext
	 * will now change and hence every group requires erasure code
	 */
	min_start_ext = cmd->ext_blocks[0].start_ext;
	for (i = 1 ; i < cmd->num_blocks; i++) {
		if (cmd->ext_blocks[i].start_ext < min_start_ext)
			min_start_ext = cmd->ext_blocks[i].start_ext;
	}

	min_start_ext = min_start_ext - (min_start_ext % MAX_DATA_STRIPES);
	/* Check the data extents from the min_start_ext to the end. Even if one
	 * of them is corrupted, we will terminate this command and notify SFS
	 * to recover the extents from the DR or its replicas and then issue this
	 * esure command
	 */
	for (j = 0; j < MAX_DATA_STRIPES; j++) {
		d_stripes[j] =
				bds_cache_alloc(sfsd_global_cache_arr[DATA64K_CACHE_OFFSET]);
		if (d_stripes[j] == NULL) {
			sfs_log(ctx, SFS_ERR, "%s(): Error getting read buffer\n",
					__FUNCTION__);
			for (k = 0; k < j; k++)
				bds_cache_free(sfsd_global_cache_arr[DATA64K_CACHE_OFFSET],
								d_stripes[k]);
			command_stat = -ENOMEM;
			goto done;
		}
	}

	for (j = 0; j < CODE_STRIPES; j++) {
		c_stripes[j] =
				bds_cache_alloc(sfsd_global_cache_arr[DATA64K_CACHE_OFFSET]);
		if (c_stripes[j] == NULL) {
			sfs_log(ctx, SFS_ERR, "%s(): Error getting read buffer\n",
					__FUNCTION__);
			for (k = 0; k < j; k++)
				bds_cache_free(sfsd_global_cache_arr[DATA64K_CACHE_OFFSET],
								c_stripes[k]);
			command_stat = -ENOMEM;
			goto done;
		}
	}

	j = 0;
	for (i = min_start_ext; i < inode->i_numextents; i++) {
		ret = get_extent(inode->i_extent, inode->i_numextents, i, NULL,
							sfsd, mount_path, ctx);
		command_stat = read_check_extent(mount_path, ERASURE_STRIPE_SIZE, TRUE,
							inode->i_extent[i].e_cksum, d_stripes[j], ctx);
		if (command_stat == -SSTACK_ECKSUM)
			goto done;
		j++;
		if (!(j % MAX_DATA_STRIPES)) {
			ret = sfs_esure_encode(d_stripes, MAX_DATA_STRIPES, c_stripes,
						CODE_STRIPES, ERASURE_STRIPE_SIZE);
			if (ret != 0) {
				command_stat = -SSTACK_FAILURE;
				goto done;
			}
			// Write c_stripes to the chunk domain
		}
	}

	if (j > 0) {
		ret = sfs_esure_encode(d_stripes, j, c_stripes,
							CODE_STRIPES, ERASURE_STRIPE_SIZE);
		if (ret != 0) {
			command_stat = -SSTACK_FAILURE;
			goto done;
		}
		// Write c_stripes to the chunk_domain
	}

done:
	for (j = 0; j < MAX_DATA_STRIPES; j++)
		bds_cache_free(sfsd_global_cache_arr[DATA64K_CACHE_OFFSET],
						d_stripes[j]);
	for (j = 0; j < CODE_STRIPES; j++)
		bds_cache_free(sfsd_global_cache_arr[DATA64K_CACHE_OFFSET],
						c_stripes[j]);

	/* Is an inode level lock required to make the change */
	if (command_stat != SSTACK_SUCCESS)
		inode->i_esure_valid = 0;
	else
		inode->i_esure_valid = 1;
	payload->response_struct.command_ok = command_stat;
	payload->command = NFS_ESURE_CODE_RSP;
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
	sfs_log(ctx, SFS_DEBUG, "%s(): command status: %d\n", __FUNCTION__,
			command_stat);

	if (command_stat != 0)
		response->response_struct.command_ok = errno;

	return response;
}

sstack_payload_t *sstack_mknod(sstack_payload_t *payload, log_ctx_t *ctx)
{
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented\n", __FUNCTION__);
	return payload;
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

	response->response_struct.command_ok = command_stat;;

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
	sfs_log(ctx, SFS_DEBUG, "%s(): command status: %d\n", __FUNCTION__,
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
	sfs_log(ctx, SFS_DEBUG, "%s(): command status: %d\n", __FUNCTION__,
			command_stat);

	if (command_stat != 0)
		response->response_struct.command_ok = errno;

	return response;
}

sstack_payload_t *sstack_readdir(sstack_payload_t *payload, log_ctx_t *ctx)
{
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented\n", __FUNCTION__);
	return payload;
}

sstack_payload_t *sstack_readdirplus(sstack_payload_t *payload, log_ctx_t *ctx)
{
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented\n", __FUNCTION__);
	return payload;
}

sstack_payload_t *sstack_fsstat(sstack_payload_t *payload, log_ctx_t *ctx)
{
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented\n", __FUNCTION__);
	return payload;
}

sstack_payload_t *sstack_fsinfo(sstack_payload_t *payload, log_ctx_t *ctx)
{
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented\n", __FUNCTION__);
	return payload;
}

sstack_payload_t *sstack_pathconf(sstack_payload_t *payload, log_ctx_t *ctx)
{
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented\n", __FUNCTION__);
	return payload;
}

sstack_payload_t *sstack_commit(sstack_payload_t *payload, log_ctx_t *ctx)
{
	/* Not implemented */
	sfs_log(ctx, SFS_INFO, "%s(): function not implemented\n", __FUNCTION__);
	return payload;
}
