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

#include <fuse.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <errno.h>
#include <attr/xattr.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <dirent.h>
#include <sfs.h>
#include <sfs_entry.h>
#include <sstack_jobs.h>
#include <sstack_md.h>
#include <sfs_job.h>
#include <sstack_helper.h>
#include <sstack_cache_api.h>
#include <sfs_jobmap_tree.h>
#include <sfs_jobid_tree.h>
#include <policy.h>

#define MAX_KEY_LEN 128


unsigned long long max_inode_number = 18446744073709551615ULL; // 2^64 -1
sstack_job_id_t curent_job_id = 0;
pthread_mutex_t sfs_job_id_mutex;
sstack_bitmap_t *sstack_job_id_bitmap = NULL;
jobmap_tree_t *jobmap_tree = NULL;
jobid_tree_t *jobid_tree = NULL;
sstack_job_id_t current_job_id = 0;

/*
 * create_payload - Allocate payload structure from payload slab and zeroizes
 *					the allocated memory.
 *
 * Retrurns valid pointer on success and NULL upon failure.
 */

static inline sstack_payload_t *
create_payload(void)
{
	sstack_payload_t *payload = NULL;

	payload = bds_cache_alloc(sfs_global_cache[PAYLOAD_CACHE_OFFSET]);
	if (NULL == payload) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to allocate payload from "
						"payload slab.\n", __FUNCTION__);
		return NULL;
	}

	memset((void *) payload, '\0', sizeof(sstack_payload_t));

	return payload;
}



/*
 * NOTE:
 * Since these routines map to their POSIX counterparts, return values are
 * set accordingly. In most cases, retun 0 on success and -1 on failure with
 * errno set accordingly.
 */

/*
 * sfs_getattr - Retrieve the attributes
 *
 * path - Path of the file for which attributes are sought
 * stbuf - stat structure. O/P parameter. Must be non-NULL
 *
 * Returns 0 on success and -1 indicating error upon failure.
 * errno will be set accordingly.
 */

int
sfs_getattr(const char *path, struct stat *stbuf)
{
	int ret = -1;
	unsigned long long inode_num = 0;
	char *inodestr = NULL;
	sstack_inode_t inode;
	size_t size = 0;

	// Parameter validation
	if (NULL == path || NULL == stbuf) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters passed. \n",
						__FUNCTION__);
		errno = EINVAL;

		return -1;
	}

	// Get the inode number for the file.
	inodestr = sstack_cache_read_one(mc, path, strlen(path), &size, sfs_ctx);
	if (NULL == inodestr) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to retrieve the reverse lookup "
					"for path %s.\n", __FUNCTION__, path);
		errno = ENOENT;

		return -1;
	}
	inode_num = atoll((const char *)inodestr);
	// Get inode from DB
	ret = get_inode(inode_num, &inode, db);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to get inode %lld. Path = %s "
						"error = %d\n", __FUNCTION__, inode_num, path, ret);
		errno = ret;

		return -1;
	}

	// Fill up stat
	stbuf->st_dev = 0; // TBD. Not sure whether this field makes sense
	stbuf->st_ino = inode.i_num;
	stbuf->st_mode = inode.i_mode;
	stbuf->st_nlink = inode.i_links;
	stbuf->st_uid = inode.i_uid;
	stbuf->st_gid = inode.i_gid;
	stbuf->st_rdev = 0; // TBD.
	stbuf->st_size = inode.i_size;
	stbuf->st_blksize = 4096; // Default file system block size
	stbuf->st_blocks = (inode.i_ondisksize / 512);
	// Stat structure only requires time in seconds.
	stbuf->st_atime = inode.i_atime.tv_sec;
	stbuf->st_mtime = inode.i_mtime.tv_sec;
	stbuf->st_ctime = inode.i_ctime.tv_sec;

	// Free up dynamically allocated fields in inode structure
	free(inode.i_xattr);
	free(inode.i_extent);
	sstack_free_erasure(sfs_ctx, inode.i_erasure, inode.i_numerasure);

	return 0;
}

/*
 * sfs_readlink - Obtain path of the symbolic link specified
 *
 * path - Path name
 * buf - Buffer to return real path. Should be non-NULL
 * size - Buffer size. Must be non-zero
 *
 * Returns 0 on success and -1 on failure with proper errno indicating error.
 */

int
sfs_readlink(const char *path, char *buf, size_t size)
{
	int ret = -1;
	unsigned long long inode_num = 0;
	char *inodestr = NULL;
	sstack_inode_t inode;
	sstack_extent_t *extent = NULL;
	sstack_file_handle_t *p = NULL;

	// Parameter validation
	if (NULL == path || NULL == buf || size == 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters passed \n",
				__FUNCTION__);
		errno = EINVAL;

		return -1;
	}

	// Get the inode number for the file.
	inodestr = sstack_cache_read_one(mc, path, strlen(path), &size, sfs_ctx);
	if (NULL == inodestr) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to retrieve the reverse lookup "
					"for path %s.\n", __FUNCTION__, path);
		errno = ENOENT;

		return -1;
	}
	inode_num = atoll((const char *)inodestr);
	// Get inode from DB
	ret = get_inode(inode_num, &inode, db);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to get inode %lld. Path = %s "
						"error = %d\n", __FUNCTION__, inode_num, path, ret);
		errno = ret;

		return -1;
	}

	/*
	 * Check if file is a symbolic link
	 * If not, return -1 .
	 * This is consistent with readink(2) behaviour
	 */

	if (inode.i_type != SYMLINK) {
		sfs_log(sfs_ctx, SFS_INFO, "%s: File specified is not a symbolic link."
						" Path = %s inode = %lld \n",
						__FUNCTION__, path, inode_num);
		errno = EINVAL;

		return -1;
	}
	extent = inode.i_extent;
	if (NULL == extent) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Fatal DB error for path %s "
						"inode %lld\n", __FUNCTION__, path, inode_num);
		errno = EFAULT;

		return -1;
	}
	// Copy first extent's path
	p = extent->e_path;
	strncpy(buf, (char *) p->name, p->name_len);

	// Free up dynamically allocated fields in inode structure
	free(inode.i_xattr);
	free(inode.i_extent);
	sstack_free_erasure(sfs_ctx, inode.i_erasure, inode.i_numerasure);

	return 0;
}

/*
 * sfs_readdir - Display files under sfs file system
 *
 * path - Path from where directory listing is required
 * buf - Buffer containing file information
 * filler
 * offset
 * fi
 *
 * NOTE:
 * Implementation is very simple here as all we need to do is perform normal
 * readdir operation. This is because sfs already contains dummy files for each
 * real file stored in the file system.
 */


int
sfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
	off_t offset, struct fuse_file_info *fi)
{
	DIR *dirpath = NULL;
	struct dirent *de = NULL;

	// Parameter validation
	if (NULL == path || NULL == buf || NULL == fi) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);
		errno = EINVAL;
		return -1;
	}

	dirpath = opendir(path);
	if (NULL == dirpath) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to open directory %s. "
						"Error = %d \n", __FUNCTION__, path, errno);
		return -1;
	}

	while ((de = readdir(dirpath)) != NULL) {
		struct stat st;

		memset(&st, '\0', sizeof(struct stat));
		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;

		if (filler(buf, de->d_name, &st, 0))
			break;
	}

	closedir(dirpath);

	return 0;
}


int
sfs_mkdir(const char *path, mode_t mode)
{

	return 0;
}


int
sfs_unlink(const char *path)
{

	return 0;
}


int
sfs_rmdir(const char * path)
{

	return 0;
}

int
sfs_symlink(const char *from, const char *to)
{

	return 0;
}

int
sfs_rename(const char *from, const char *to)
{

	return 0;
}

int
sfs_link(const char *from, const char *to)
{

	return 0;
}

int
sfs_chmod(const char *path, mode_t mode)
{

	return 0;
}

int
sfs_chown(const char *path, uid_t uid, gid_t gid)
{

	return 0;
}

int
sfs_truncate(const char *path, off_t size)
{

	return 0;
}


int
sfs_open(const char *path, struct fuse_file_info *fi)
{

	return 0;
}

int
sfs_read(const char *path, char *buf, size_t size, off_t offset,
	struct fuse_file_info *fi)
{
	unsigned long long inode_num = 0;
	char *inodestr = NULL;
	sstack_inode_t inode;
	size_t size1 = 0;
	sstack_extent_t *extent = NULL;
	int i = 0;
	int bytes_to_read = 0;
	sstack_payload_t *payload = NULL;
	pthread_t thread_id;
	sstack_job_map_t *job_map = NULL;
	off_t relative_offset = 0;
	size_t bytes_issued = 0;
	int ret = -1;
	policy_entry_t *policy = NULL;
	sfsd_list_t *sfsds = NULL;

	// Paramater validation
	if (NULL == path || NULL == buf || size < 0 || offset < 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters \n",
				__FUNCTION__);
		errno = EINVAL;

		return -1;
	}

	// Get the inode number for the file.
	inodestr = sstack_cache_read_one(mc, path, strlen(path), &size1, sfs_ctx);
	if (NULL == inodestr) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to retrieve the reverse lookup "
					"for path %s.\n", __FUNCTION__, path);
		errno = ENOENT;

		return -1;
	}
	inode_num = atoll((const char *)inodestr);
	// Get inode from DB
	ret = get_inode(inode_num, &inode, db);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to get inode %lld. Path = %s "
						"error = %d\n", __FUNCTION__, inode_num, path, ret);
		errno = ret;

		return -1;
	}

	// Parameter validation; AGAIN :-)
	if (inode.i_size < size || (offset + size ) > inode.i_size ) {
		// Appl asking for file offset greater tha real size
		// Return error
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid offset/size specified \n",
				__FUNCTION__);
		errno = EINVAL;

		return -1;
	}

	relative_offset = offset;
	// Get the sfsd information from IDP
	sfsds = sfs_idp_get_sfsd_list(&inode, sfsd_pool, sfs_ctx);
	if (NULL == sfsds) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: sfs_idp_get_sfsd_list failed \n",
				__FUNCTION__);
		return -1;
	}

	thread_id = pthread_self();

	// Get the extent covering the request
	for(i = 0; i < inode.i_numextents; i++) {
		extent = inode.i_extent;
		if (extent->e_offset <= relative_offset &&
				(extent->e_offset + extent->e_size >= relative_offset)) {
			// Found the initial extent
			break;
		}
		extent ++;
	}

	bytes_to_read = size;
	// Create job_map for this job.
	// This is required to track multiple sub-jobs to a single request
	// This is safe to do as async IO from applications are not part of
	// FUSE framework. So this is the only outstanding request from the
	// thread. Moreover the thread waits on a condition variable after
	// submitting jobs.
	// This is useful only in case of requests that span across multiple
	// extents. Idea is to simplify sfsd functionality by splitting the
	// IO at extent boundary
	job_map = create_job_map();
	if (NULL == job_map) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed allocate memory for "
						"job map \n", __FUNCTION__);
		return -1;
	}
	/*
	 * Note: Don't free policy. It is just a pointer to original DS
	 * and no copies are made by get_policy.
	 */
	policy = get_policy(path);
	if (NULL == policy) {
		sfs_log(sfs_ctx, SFS_INFO, "%s: No policies specified for the file"
				". Default policy applied \n", __FUNCTION__);
		policy->pe_attr.a_qoslevel = QOS_LOW;
		policy->pe_attr.a_ishidden = 0;
	} else {
		sfs_log(sfs_ctx, SFS_INFO, "%s: path %s ver %s qoslevel %d "
				"hidden %d \n", __FUNCTION__, path, policy->pe_attr.ver,
				policy->pe_attr.a_qoslevel, policy->pe_attr.a_ishidden);
	}

	while (bytes_to_read) {
		sfs_job_t *job = NULL;
		int j = -1;
		size_t read_size = 0;
		char *temp = NULL;

		// Submit job
		job = sfs_job_init();
		if (NULL == job) {
			sfs_log(sfs_ctx, SFS_ERR, "%s: Failed allocate memory for job.\n",
					__FUNCTION__);
			free_job_map(job_map);
			return -1;
		}

		job->id = get_next_job_id();
		job->job_type = SFSD_IO;
		job->num_clients = sfsds->num_sfsds;
		memcpy((void *) &job->sfsds, (void *) sfsds->sfsds, sizeof(sfsd_t *) *
				sfsds->num_sfsds);
		for (j = 0; j < sfsds->num_sfsds; j++)
			job->job_status[j] =  JOB_STARTED;


		job->priority = policy->pe_attr.a_qoslevel;
		// Create new payload
		payload = create_payload();
		// Populate payload
		payload->hdr.sequence = 0; // Reinitialized by transport
		payload->hdr.payload_len = sizeof(sstack_payload_t);
		payload->hdr.job_id = job->id;
		payload->hdr.priority = job->priority;
		payload->hdr.arg = (uint64_t) job;
		payload->command = NFS_READ;
		payload->command_struct.read_cmd.inode_no = inode.i_num;
		payload->command_struct.read_cmd.offset = offset;
		if ((offset + size) > (extent->e_offset + extent->e_size))
			read_size = (extent->e_offset + extent->e_size) - offset;
		payload->command_struct.read_cmd.count = read_size;
		payload->command_struct.read_cmd.read_ecode = 0;
		memcpy((void *) &payload->command_struct.read_cmd.pe, (void *)
						policy, sizeof(struct policy_entry));
		job->payload_len = sizeof(sstack_payload_t);
		job->payload = payload;
		// Add this job to job_map
		job_map->num_jobs ++;
		temp = realloc(job_map->job_ids, job_map->num_jobs *
						sizeof(sstack_job_id_t));
		if (NULL == temp) {
			sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to allocate memory for "
							"job_ids \n", __FUNCTION__);
			if (job_map->job_ids)
				free(job_map->job_ids);
			free_job_map(job_map);
			free_payload(sfs_global_cache, payload);

			return -1;
		}
		job_map->job_ids = (sstack_job_id_t *) temp;

		pthread_spin_lock(&job_map->lock);
		job_map->job_ids[job_map->num_jobs - 1] = job->id;
		pthread_spin_unlock(&job_map->lock);

		temp = realloc(job_map->job_status, job_map->num_jobs *
						sizeof(sstack_job_status_t));
		if (NULL == temp) {
			sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to allocate memory for "
							"job_status \n", __FUNCTION__);
			free(job_map->job_ids);
			if (job_map->job_status)
				free(job_map->job_status);
			free_job_map(job_map);
			free_payload(sfs_global_cache, payload);

			return -1;
		}
		pthread_spin_lock(&job_map->lock);
		job_map->job_status[job_map->num_jobs - 1] = JOB_STARTED;
		pthread_spin_unlock(&job_map->lock);

		ret = sfs_job2thread_map_insert(thread_id, job->id);
		if (ret == -1) {
			sfs_log(sfs_ctx, SFS_ERR, "%s: Failed insert the job context "
							"into RB tree \n", __FUNCTION__);
			free(job_map->job_ids);
			free(job_map->job_status);
			sfs_job_context_remove(thread_id);
			free_job_map(job_map);
			free_payload(sfs_global_cache, payload);

			return -1;
		}
		// Add this job to job queue
		ret = sfs_submit_job(job->priority, jobs, job);
		if (ret == -1) {
			free(job_map->job_ids);
			free(job_map->job_status);
			sfs_job_context_remove(thread_id);
			sfs_job2thread_map_remove(thread_id);
			free_job_map(job_map);
			free_payload(sfs_global_cache, payload);

			return -1;
		}

		bytes_issued += read_size;
		// Check whether we are done with original request
		if ((offset + size) > (extent->e_offset + extent->e_size)) {
			// Request spans multiple extents
			extent ++;
			// TODO
			// Check for sparse file condition
			relative_offset = extent->e_offset;
			bytes_to_read = size - bytes_issued;
		} else {
			bytes_to_read = size - bytes_issued;
		}
	}
	// Add job_map to the jobmap RB-tree
	ret = sfs_job_context_insert(thread_id, job_map);
	if (ret == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed insert the job context "
						"into RB tree \n", __FUNCTION__);
		free(job_map->job_ids);
		free(job_map->job_status);
		free_job_map(job_map);
		free_payload(sfs_global_cache, payload);

		return -1;
	}

	ret = sfs_wait_for_completion(job_map);
	// TODO
	// Handle completion status

	return 0;
}

int
sfs_write(const char *path, const char *buf, size_t size, off_t offset,
	struct fuse_file_info *fi)
{

	return 0;
}

/*
 * sfs_statfs - Provide file system statistics
 */

int
sfs_statfs(const char *path, struct statvfs *buf)
{

	// Parameter validation
	if (NULL == path || NULL == buf) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified\n",
						__FUNCTION__);
		errno = -EINVAL;

		return -1;
	}
	// Fill in some fields
	buf->f_bsize = 4096;
	buf->f_frsize = 4096; // Does not support fragments
	// TODO
	// f_blocks and f_bfree are to calculated after contacting sfsds
	// Pseudocode:
	// 1> Create a job structure with GET_STATS operation. This will be
	// a new storage operation
	// 2> Loop through all the sfsds registered with this sfs by going through
	// sfs_metadata->info. Transport information of each sfsd is embedded
	// in sfs_info which is inturn embedded in sfs_metadata
	// 3> sfsds need to provide total blocks and free blocks to sfs. This
	// information is available in chunk domain structure.
	// 4> sfs then adds up total blocks and free blocks and fills up
	// buf->f_blocks and buf->f_bfree.
	buf->f_bavail = ((buf->f_blocks * 95) / 100); // Assuming 5% quota for root
	pthread_mutex_lock(&inode_mutex);
	buf->f_files = active_inodes;
	pthread_mutex_unlock(&inode_mutex);
	buf->f_bfree = (unsigned int) (max_inode_number - buf->f_files);
	buf->f_favail = ((buf->f_bfree * 95) / 100); // 5% exclusive quota for root
	buf->f_fsid = SFS_MAGIC; // ambiguous in man page
	// TBD
	// buf->f_flag contains mount flags
	buf->f_flag = 0; // ST_RDONLY to be set only if mounted readonly
	buf->f_namemax = 4096;

	return 0;
}

int
sfs_flush(const char *path, struct fuse_file_info *fi)
{

	return 0;
}

/*
 * sfs_release - Release the file handle
 *
 * path - Pathname of the file
 * fi - FUSE file handle
 *
 * Close the file handle stored in fi
 * Retrun 0 if parameters are valid. Otherwise retruns -1 and sets errno
 */
int
sfs_release(const char *path, struct fuse_file_info *fi)
{
	int res = -1;

	// Parameter validation
	if (NULL == path || NULL == fi) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified.\n",
						__FUNCTION__);
		errno = EINVAL;

		return -1;
	}

	// Close the file descriptor
	res = close(fi->fh);
	if (res == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Close on file %s failed with "
						"error %d\n", __FUNCTION__, path, errno);

		return -1;
	}
	// Remove the reverse mapping for the path
	res = sstack_cache_remove(mc, path, sfs_ctx);
	if (res != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Reverse lookup for path %s failed. "
						"Return value = %d\n", __FUNCTION__, path, res);
	}

	return 0;
}


int
sfs_fsync(const char *path, int isdatasync, struct fuse_file_info *fi)
{

	return 0;
}

/*
 * replace_xattr - Replace the existing attribute value with new value
 *
 * inode - Inode structure corrresponding to target file
 * name - Extended attribute name
 * value - New value to be stored
 * len - Size of new attribute
 *
 * Returns 0 on success and -1 on failure. Sets errno accordingly in case of
 * error.
 *
 * NOTE:
 * This imp is quite tedious and ugly. Couldn't use strchr/strtok as
 * This string is actually an array of strings and there is no easy
 * way to do it without storing into intermediate data structure.
 * This code is tested as a standalone and it works :-)
 */

static int
replace_xattr(sstack_inode_t *inode, const char *name, const char *value,
				size_t len)
{
	int i = 0;
	char *p = NULL;
	char key[MAX_KEY_LEN] = { '\0' };
	int j;
	int ret = -1;

	// Parameter validation
	if (NULL == inode ||  NULL == name || NULL == value || len == 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);
		errno = EINVAL;

		return -1;
	}

	p = inode->i_xattr;
	while ( i < inode->i_xattrlen) {

		j = 0;
		memset(key, '\0', 128);
		if (*(p + i) == '\0') {
			i++;
			continue;
		}
		// Copy till '=' into key
		while (*(p + i) != '=') {
			*(key + i) = *(p + i);
			i++;
			j++;
		}
		key[j + 1] = '\0';

		if (strcmp(key, name) == 0) {
			// Match found
			break;
		}
		i += j;
		// Skip over value
		while(*(p + i) != '\0')
			i++;
	}
	// Extended attr found
	// Check the size of the value
	i++; // Step over '='
	// Get size of value
	j = 0;
	while (*(p + i ) != '\0') {
		j++;
		i++;
	}

	i -= j;
	// Size of original value to be replaced is j
	// Adjust i_xattr
	if (len < j) {
		// Copy inplace
		memmove((void *) (p + i), (void *) value, len);
		i += len;
		memmove((void *) (p + i), (void *) (p + i + j - len),
						(inode->i_xattrlen - (j - len)));

		inode->i_xattrlen = inode->i_xattrlen -j + len;
	} else if (len == j) {
		memmove((void *) (p + i) , (void *) value, len);
		// There is no change in length
	} else {
		char * temp = NULL;

		// New value is larger than original value
		temp = realloc(inode->i_xattr, inode->i_xattrlen + (len - j));
		if (NULL == temp) {
			sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to allocate memory \n",
					__FUNCTION__);
			errno = ENOMEM;

			return -1;
		}
		// Make space for new value
		memmove((void *) (temp + i + len), (void *) (temp + i + j),
				(inode->i_xattrlen - (j - len)));
		memmove((void *) (temp + i), (void *) value, len);

		inode->i_xattrlen += (len - j);
		inode->i_xattr = temp;
	}

	// Store the updated inode
	ret = put_inode(inode, db);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: put inode failed. Error = %d\n",
						__FUNCTION__, errno);
		// Don't modify errno set by put_inode
		return -1;
	}

	return 0;
}

/*
 * append_xattr - Append the xattr to end of i_xattr
 *
 * inode - inode structure of the target file
 * name - Extended attribute name
 * value - Extended attribute value
 * size - extended attribute value size
 *
 * Returns 0 on success and -1 on failure and updates errno
 */

static int
append_xattr(sstack_inode_t *inode, const char * name, const char * value,
				size_t len)
{
	char *p = NULL;
	int key_len = 0;
	int cur_len = 0;
	int ret = -1;

	// Parameter validation
	if (NULL == inode || NULL == name || NULL == value || len == 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified. \n",
						__FUNCTION__);
		errno = EINVAL;

		return -1;
	}

	// Restricting key length to MAX_KEY_LEN to avoid buffer overflows
	if (strlen(name) > MAX_KEY_LEN)
		key_len = MAX_KEY_LEN;

	// We need key_len + len + one for '=' +  one for '\0'
	p = realloc(inode->i_xattr,
					(inode->i_xattrlen + key_len + len + 1 + 1));
	if (NULL == p) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Out of memory \n", __FUNCTION__);
		errno = ENOMEM;

		return -1;
	}
	// Append
	// This can be rewritten to use sprintf
	cur_len = inode->i_xattrlen;
	// Append key
	// Omit '\0' of key
	memcpy((void *) (p + cur_len) , (void *) name, key_len);
	cur_len  += key_len;
	// Append '='
	memcpy((void *) (p + cur_len) , (void *)  "=", 1);
	cur_len += 1;
	// Append value
	// Value is anyway a string. So directly strcpy it.
	// It is assumed that len includes trailing '\0' character.
	// Manpage is amibuous.
	strcpy((p + cur_len), value);
	cur_len += len;

	inode->i_xattrlen = cur_len;
	inode->i_xattr = p;

	// Store the updated inode
	ret = put_inode(inode, db);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: put inode failed. Error = %d\n",
						__FUNCTION__, errno);
		// Don't modify errno set by put_inode
		return -1;
	}

	return 0;
}

/*
 * xattr_exists - Helper function  to check existence of attr in inode
 *
 * inode - Inode of the file. Must be non-NULL
 * name - Extended attribute name
 *
 * Returns 1 if attr exists and negative number otherwise.
 */

static int
xattr_exists(sstack_inode_t *inode, const char * name)
{
	int i = 0;
	char *p = NULL;
	char key[MAX_KEY_LEN] = { '\0' };

	if (NULL == inode || NULL == name || NULL == inode->i_xattr ||
					inode->i_xattrlen == 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters passed. \n",
					__FUNCTION__);
		errno = EINVAL;

		return -1;
	}
	p = inode->i_xattr;
	// Walk through the xattrs
	while ( i < inode->i_xattrlen) {
		int j;

		j = 0;
		memset(key, '\0', MAX_KEY_LEN);
		// Skip field separators (NULL character)
		if (*(p + i) == '\0') {
			i++;
			continue;
		}
		// Copy till '=' into key
		while (*(p + i) != '=') {
			*(key + i) = *(p + i);
			i++;
			j++;
		}

		key[j + 1] = '\0';

		if (strcmp(key, name) == 0) {
			// Match found
			return 1;
		}
		i += j;
		// Skip over value
		while(*(p + i) != '\0')
			i++;
	}

	return -1;
}


/*
 * sfs_setxattr - Set extended attributes
 *
 * path - Path name of the target file
 * name - Name of the attribute
 * value - Value to be assigned to attribute
 * size - size of the attribute
 * flags - XATTR_CREATE or XATTR_REPLACE or 0
 *
 * Returns 0 on success and -1 on failure.
 */

int
sfs_setxattr(const char *path, const char *name, const char *value,
	size_t len, int flags)
{
	int ret = -1;
	unsigned long long inode_num = 0;
	char *inodestr = NULL;
	sstack_inode_t inode;
	size_t size;

	// Parameter validation
	if (NULL == path || NULL == name || NULL == value || size == 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters passed. \n",
					__FUNCTION__);
		errno = EINVAL;

		return -1;
	}

	// Get the inode number for the file.
	inodestr = sstack_cache_read_one(mc, path, strlen(path), &size, sfs_ctx);
	if (NULL == inodestr) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to retrieve the reverse lookup "
					"for path %s.\n", __FUNCTION__, path);
		errno = ENOENT;

		return -1;
	}
	inode_num = atoll((const char *)inodestr);
	// Get inode from DB
	ret = get_inode(inode_num, &inode, db);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to get inode %lld. Path = %s "
						"error = %d\n", __FUNCTION__, inode_num, path, ret);
		errno = ret;

		return -1;
	}

	// Walk through the inode.i_xattr list if flags is non-zero
	switch (flags) {
		case XATTR_CREATE:
			// Check if the attribute exists already
			// If so, return
			if (xattr_exists(&inode, name) == 1) {
				sfs_log(sfs_ctx, SFS_ERR, "%s: Attribute %s already exists.",
								" Path = %s inode %lld \n",
								__FUNCTION__, name, path, inode_num);
				errno = EEXIST;

				return -1;
			} else {
				ret = append_xattr(&inode, name, value, len);
				if (ret == -1) {
					sfs_log(sfs_ctx, SFS_ERR, "%s: Attribute create failed for"
								" file %s inode %lld . Error = %d \n",
								__FUNCTION__, path, inode_num, errno);
					return -1;
				} else
					return 0;
			}
		case XATTR_REPLACE:
			// Check if attribute exists
			// If not, return error
			if (xattr_exists(&inode, name) != 1)  {
				sfs_log(sfs_ctx, SFS_ERR, "%s: Attribute %s does not exist.",
								" Path = %s inode %lld attr %s\n",
								__FUNCTION__, name, path, inode_num, name);
				errno = ENOATTR;

				return -1;
			} else {
				ret = replace_xattr(&inode, name, value, len);
				if (ret == -1) {
					sfs_log(sfs_ctx, SFS_ERR, "%s: Attribute replace failed for"
								" file %s inode %lld . Error = %d \n",
								__FUNCTION__, path, inode_num, errno);
					return -1;
				} else
					return 0;
			}
		case 0:
			if (xattr_exists(&inode, name) == 1) {
				ret = replace_xattr(&inode, name, value, len);
				if (ret == -1) {
					sfs_log(sfs_ctx, SFS_ERR, "%s: Attribute replace failed for"
								" file %s inode %lld . Error = %d \n",
								__FUNCTION__, path, inode_num, errno);
					return -1;
				} else
					return 0;
			} else {
				ret = append_xattr(&inode, name, value, len);
				if (ret == -1) {
					sfs_log(sfs_ctx, SFS_ERR, "%s: Attribute create failed for"
								" file %s inode %lld . Error = %d \n",
								__FUNCTION__, path, inode_num, errno);
					return -1;
				} else
					return 0;
			}
		default:
			sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid flags specified \n");
			errno = EINVAL;

			return -1;
	}
}


/*
 * sfs_getxattr - Get extended attributes
 *
 * path - Path name of the target file
 * name - Name of the attribute
 * value - Value to be read
 * size - size of the value
 *
 * Returns 0 on success and -1 on failure and sets errno.
 */
int
sfs_getxattr(const char *path, const char *name, char *value, size_t size)
{

	int ret = -1;
	unsigned long long inode_num = 0;
	char *inodestr = NULL;
	sstack_inode_t inode;
	size_t size1;
	int i = 0;
	char *p = NULL;
	char key[MAX_KEY_LEN] = { '\0' };

	// Parameter validation
	if (NULL == path || NULL == name || NULL == value ||
					size == 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters passed. \n",
					__FUNCTION__);
		errno = EINVAL;

		return -1;
	}

	// Get the inode number for the file.
	inodestr = sstack_cache_read_one(mc, path, strlen(path), &size1, sfs_ctx);
	if (NULL == inodestr) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to retrieve the reverse lookup "
					"for path %s.\n", __FUNCTION__, path);
		errno = ENOENT;

		return -1;
	}
	inode_num = atoll((const char *)inodestr);
	// Get inode from DB
	ret = get_inode(inode_num, &inode, db);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to get inode %lld. Path = %s "
						"error = %d\n", __FUNCTION__, inode_num, path, ret);
		errno = ret;

		return -1;
	}

	p = inode.i_xattr;
	// Walk through the xattrs
	while ( i < inode.i_xattrlen) {
		int j;

		j = 0;
		memset(key, '\0', MAX_KEY_LEN);
		// Skip field separators (NULL character)
		if (*(p + i) == '\0') {
			i++;
			continue;
		}
		// Copy till '=' into key
		while (*(p + i) != '=') {
			*(key + i) = *(p + i);
			i++;
			j++;
		}

		key[j + 1] = '\0';

		if (strcmp(key, name) == 0) {
			int k = 0;
			// Copy xattr value into value
			// Manpage does not mention about size paramater.
			// Assuming size parameter does not including trailing '\0'
			// Copy either size bytes or till we hit field separator ('\0')
			while ((*(p + i) != '\0') &&  k < size) {
				*(value + k) = *(p + i);
				i ++;
				k ++;
			}

			return k;
		}
		i += j;
		// Skip over value
		while(*(p + i) != '\0')
			i++;
	}

	// If we did not get a match, error out.
	sfs_log(sfs_ctx, SFS_ERR, "%s: Attribute %s not found for path %s\n",
					__FUNCTION__, name, path);
	errno = ENOATTR;

	return -1;
}

/*
 * sfs_listxattr - Get extended attributes in a buffer
 *
 * path - Path name of the target file
 * list - Pre-allocated buffer to hold xattrs
 * size - size of the value
 *
 * Returns 0 on success and -1 on failure and sets errno.
 */

int
sfs_listxattr(const char *path, char *list, size_t size)
{
	int ret = -1;
	unsigned long long inode_num = 0;
	char *inodestr = NULL;
	sstack_inode_t inode;
	size_t size1;
	int i = 0;
	char *p = NULL;
	char key[MAX_KEY_LEN] = { '\0' };
	int bytes_copied = 0;


	// Parameter validation
	if (NULL == path || NULL == list || size == 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);
		errno = EINVAL;

		return -1;
	}

	// Get the inode number for the file.
	inodestr = sstack_cache_read_one(mc, path, strlen(path), &size1, sfs_ctx);
	if (NULL == inodestr) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to retrieve the reverse lookup "
					"for path %s.\n", __FUNCTION__, path);
		errno = ENOENT;

		return -1;
	}
	inode_num = atoll((const char *)inodestr);
	// Get inode from DB
	ret = get_inode(inode_num, &inode, db);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to get inode %lld. Path = %s "
						"error = %d\n", __FUNCTION__, inode_num, path, ret);
		errno = ret;

		return -1;
	}

	p = inode.i_xattr;
	// Walk through the xattrs
	while ( i < inode.i_xattrlen) {
		int j;

		j = 0;
		memset(key, '\0', MAX_KEY_LEN);
		// Skip field separators (NULL character)
		if (*(p + i) == '\0') {
			i++;
			continue;
		}
		// Copy till '=' into key
		while (*(p + i) != '=') {
			*(key + i) = *(p + i);
			i++;
			j++;
		}

		key[j + 1] = '\0';
		// Value contains concatenation of NULL terminated strings
		// like user.name1\0user.name2\0user.name3 ...
		if (size > bytes_copied && ((bytes_copied + (j + 1)) < size)) {
			strcpy((char *) (list + bytes_copied), key);
			bytes_copied += (j + 1);
		} else {
			// List filled up.
			break;
		}

		i += j;
		// Skip over attribute value
		while(*(p + i) != '\0')
			i++;
	}

	return bytes_copied;
}

/*
 * xattr_remove - Remove the specified attribute from inode
 *
 * inode - Inode structure . Should be non-NULL.
 * name - attribute name. Should be non-NULL.
 *
 * Removes the specified attribute from the extended attributes stored in the
 * inode.
 *
 * Returns size of updated xattr array upon success and -1 upon failure.
 * Sets errno accordingly.
 */

static int
xattr_remove(sstack_inode_t *inode, const char *name)
{
	int i = 0;
	char key[MAX_KEY_LEN] = { '\0' };
	int key_size = 0;
	int value_size = 0;
	int pos = 0;
	int pair_len = 0;
	int len = 0;
	char *p = NULL;
	int found = 0;

	// Parameter validation
	if (NULL == inode || NULL == name) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);
		errno = EINVAL;
		return -1;
	}

	if (NULL == inode->i_xattr || inode->i_xattrlen <= 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);
		errno = EINVAL;
		return -1;
	}


	len = inode->i_xattrlen;
	p = inode->i_xattr;

	while ( i < len) {
		int j;

		j = 0;
		memset(key, '\0', MAX_KEY_LEN);
		if (*(p + i) == '\0') {
			i++;
			continue;
		}
		pos = i; // Position of key
		// Read in the key
		while (*(p + i) != '=') {
			*(key + j) = *(p + i);
			i++;
			j++;
		}
		key[j + 1] = '\0';
		key_size = strlen(key);

		// Check if this the attr to be removed
		if (strcmp(name, key) == 0) {
			// Found
			found = 1;
			break;
		}
		i += key_size;
		// Skip the value field
		while (*(p + i) != '\0')
			i++;
	}

	if (found == 0) {
		// Key not found
		errno = ENOATTR;
		return -1;
	}

	// Key found
	// Check size of value
	i++; // Step over '='
	value_size = 0;
	while (*(p + i) != '\0') {
		value_size ++;
		i++;
	}
	// Remove this {key, value} pair
	// Adjust i_xattr and i_xattrlen
	pair_len = key_size + 1 + value_size + 1; // One for '=' and one for '\0'
	if (i == (len - 1)) {
		// This key=value pair is the last one in i_xattr
		// Simply adjust i_xattrlen;
		return (len - pair_len);
	} else if (key[0] ==*p) {
		// This key=value pair is the first one in i_xattr
		// shift the rest by pair_len and adjust i_xattrlen
		memmove((void *) p, (void *) (p + pair_len), (len - pair_len));
		return (len - pair_len);
	} else {
		// This key=value pair is neitherthe fist nor the last
		memmove((void *) (p + pos), (void *) (p + pos + pair_len),
						(len - pair_len));
		return (len - pair_len);
	}
}

/*
 * sfs_removexattr - Remove an extended attribute
 *
 * path - Path name of the target file
 * name - Extended attribute name to be removed
 *
 * Returns 0 on success and -1 on failure and sets errno accordingly.
 */
int
sfs_removexattr(const char *path, const char *name)
{
	int ret = -1;
	unsigned long long inode_num = 0;
	char *inodestr = NULL;
	sstack_inode_t inode;
	size_t size;

	// Parameter validation
	if (NULL == path || NULL == name) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters passed. \n",
					__FUNCTION__);
		errno = EINVAL;

		return -1;
	}

	// Get the inode number for the file.
	inodestr = sstack_cache_read_one(mc, path, strlen(path), &size, sfs_ctx);
	if (NULL == inodestr) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to retrieve the reverse lookup "
					"for path %s.\n", __FUNCTION__, path);
		errno = ENOENT;

		return -1;
	}
	inode_num = atoll((const char *)inodestr);
	// Get inode from DB
	ret = get_inode(inode_num, &inode, db);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to get inode %lld. Path = %s "
						"error = %d\n", __FUNCTION__, inode_num, path, ret);
		errno = ret;

		return -1;
	}


	ret = xattr_remove(&inode, name);
	if (ret == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Removing xattr %s failed for file %s\n",
						__FUNCTION__, name, path);
		errno = ENOATTR;
		return -1;
	}

	// Update inode
	ret = put_inode(&inode, db);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: put inode failed. Error = %d\n",
						__FUNCTION__, errno);
		// Don't modify errno set by put_inode
		return -1;
	}

	return 0;
}

int
sfs_opendir(const char *path, struct fuse_file_info *fi)
{

	return 0;
}

int
sfs_releasedir(const char *path, struct fuse_file_info *fi)
{

	return 0;
}

int
sfs_fsyncdir(const char *path, int isdatasync, struct fuse_file_info *fi)
{

	return 0;
}

void
sfs_destroy(void *arg)
{

}

/*
 * sfs_access - Check access permissions for the file
 *
 * path - Path name of the target file
 * mode - access permissions to be checked
 *
 * Returns 0 is access permissions specified in mode are set.
 * If mode is F_OK, returns 0 if path exists.
 * Returns -1 on failure and sets errno accordingly.
 */
int
sfs_access(const char *path, int mode)
{
	unsigned long long inode_num = 0;
	char *inodestr = NULL;
	sstack_inode_t inode;
	size_t size = 0;
	int ret = -1;

	// Parameter validation
	if (NULL == path || mode == 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);
		errno = EINVAL;

		return -1;
	}

	// Get the inode number for the file
	inodestr = sstack_cache_read_one(mc, path, strlen(path), &size, sfs_ctx);
	if (NULL == inodestr) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to retrieve the reverse lookup "
						"for path %s.\n", __FUNCTION__, path);
		errno = ENOENT;

		return -1;
	}
	inode_num = atoll((const char *) inodestr);
	// Get inode from db
	ret = get_inode(inode_num, &inode, db);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to get inode %lld. Path = %s "
						"error = %d\n", __FUNCTION__, inode_num, path, ret);
		errno = ret;

		return -1;
	}

	if (mode & F_OK) {
		// Application is checking whether file exists
		// Since file info is in db, file exists.
		free(inode.i_xattr);
		free(inode.i_extent);

		return 0;
	}

	if (inode.i_mode & mode) {
		// Mode is a subset of what is set in inode
		free(inode.i_xattr);
		free(inode.i_extent);

		return 0;
	 } else {
		free(inode.i_xattr);
		free(inode.i_extent);
		errno = EACCES;

		return  -1;
	}
}

/*
 * sfs_create - create(2) handler for sfs
 *
 * path - Full path of the file to be created
 * mode - Mode of creation
 * fi -  FUSE file handle
 *
 */

int
sfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	sstack_inode_t inode;
	struct stat status;
	int ret = -1;
	sstack_file_handle_t *ep = NULL;
	char inode_str[MAX_INODE_LEN] = { '\0' };

    sfs_log(sfs_ctx, SFS_DEBUG, "%s: path = %s\n", __FUNCTION__, path);
	// Parameter validation
	if (NULL == path) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified \n",
				__FUNCTION__);
		errno = EINVAL;
		return -1;
	}

	ret = creat(path, mode);
	if (ret == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Create failed with error %d \n",
				__FUNCTION__, errno);
		errno = EACCES;
		return -1;
	}

	// Populate DB with new inode info
	inode.i_num = get_free_inode();
	strcpy(inode.i_name, path);
	inode.i_uid = status.st_uid;
	inode.i_gid = status.st_gid;
	inode.i_mode = status.st_mode;
	switch (status.st_mode & S_IFMT) {
		case S_IFDIR:
			inode.i_type = DIRECTORY;
			break;
		case S_IFREG:
			inode.i_type = REGFILE;
			break;
		case S_IFLNK:
			inode.i_type = SYMLINK;
			break;
		case S_IFBLK:
			inode.i_type = BLOCKDEV;
			break;
		case S_IFCHR:
			inode.i_type = CHARDEV;
			break;
		case S_IFIFO:
			inode.i_type = FIFO;
			break;
		case S_IFSOCK:
			inode.i_type = SOCKET_TYPE;
			break;
		default:
			inode.i_type = UNKNOWN;
			break;
	}

	// Make sure we are not looping
	// TBD

	memcpy(&inode.i_atime, &status.st_atime, sizeof(struct timespec));
	memcpy(&inode.i_ctime, &status.st_ctime, sizeof(struct timespec));
	memcpy(&inode.i_mtime, &status.st_mtime, sizeof(struct timespec));
	inode.i_size = status.st_size;
	inode.i_ondisksize = (status.st_blocks * 512);
	inode.i_numreplicas = 1; // For now, single copy
	// Since the file already exists, done't split it now. Split it when
	// next write arrives
	inode.i_numextents = 1;
	inode.i_numerasure = 0; // No erasure code segments
	inode.i_xattrlen = 0; // No extended attributes
	inode.i_links = status.st_nlink;
	sfs_log(sfs_ctx, SFS_INFO,
		"%s: nlinks for %s are %d\n", __FUNCTION__, path, inode.i_links);
	// Populate the extent
	inode.i_extent = NULL;
	inode.i_erasure = NULL; // No erasure coding info for now
	inode.i_xattr = NULL; // No extended attributes carried over

	// Store inode
	ret = put_inode(&inode, db);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to store inode #%lld for "
				"path %s. Error %d\n", __FUNCTION__, inode.i_num,
				inode.i_name, errno);
		return -1;
	}

	free(ep);

	// Populate memcahed for reverse lookup
	sprintf(inode_str, "%lld", inode.i_num);
	ret = sstack_cache_store(mc, path, inode_str, (strlen(inode_str) + 1),
					sfs_ctx);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to store object into memcached."
				" Key = %s value = %s \n", __FUNCTION__, path, inode_str);
		return -1;
	}

	return 0;
}

int
sfs_ftruncate(const char *path, off_t offset, struct fuse_file_info *fi)
{

	return 0;
}

int
sfs_fgetattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{

	return 0;
}


int
sfs_lock(const char *path, struct fuse_file_info *fi, int cmd,
			struct flock *flock)
{

	return 0;
}

int
sfs_utimens(const char *path, const struct timespec tv[2])
{

	return 0;
}

int
sfs_flock(const char *path, struct fuse_file_info *fi, int op)
{

	return 0;
}

//==========================================================================//
// 		FOLLOWING ENTRY POINTS ARE NOT IMPLEMENTED
//==========================================================================//

/*
 *  Provide block map (as in FIBMAP) for the given offset
 *  Can not be implemented.
 */
int
sfs_bmap(const char *path, size_t blocksize, uint64_t *idx)
{
	errno = ENOSYS;
	return -1;
}

int
sfs_ioctl(const char *path, int cmd, void *arg, struct fuse_file_info *fi,
				unsigned int flags, void *data)
{

	return 0;
}

int
sfs_poll(const char *path, struct fuse_file_info *fi,
				struct fuse_pollhandle *ph, unsigned *reventsp)
{

	return 0;
}

int
sfs_write_buf(const char *path, struct fuse_bufvec *buf, off_t off,
				struct fuse_file_info *fi)
{

	errno = ENOSYS;
	return -1;
}

int
sfs_read_buf(const char *path, struct fuse_bufvec **bufp, size_t size,
				off_t off, struct fuse_file_info *fi)
{
	errno = ENOSYS;
	return -1;

}
int
sfs_mknod(const char *path, mode_t mode, dev_t rdev)
{
	errno = ENOSYS;
	return -1;
}

