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

#include <fuse.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
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
#include <sfs_entry.h>
#include <policy.h>
#include <bds_slab.h>
#include <sstack_serdes.h>

#define MAX_KEY_LEN 128
#define SFS_DIR "/var/sfs"


unsigned long long max_inode_number = 18446744073709551615ULL; // 2^64 -1
sstack_job_id_t curent_job_id = 0;
pthread_mutex_t sfs_job_id_mutex;
sstack_bitmap_t *sstack_job_id_bitmap = NULL;
sstack_job_id_t current_job_id = 0;
extern bds_slab_desc_t *serdes_caches;

extern char sfs_mountpoint[];

static inline int
sfs_send_read_status(sstack_job_map_t *job_map, char *buf, size_t size);
static inline int
sfs_send_write_status(sstack_job_map_t *job_map, sstack_inode_t inode,
							off_t offset, size_t size);

static char *
prepend_mntpath(const char *path)
{
	char *fullpath = NULL;

	// Parameter validation
	if (NULL == path)
		return NULL;

	fullpath = malloc(strlen(path) + strlen(SFS_DIR) + 1);
	if (NULL == fullpath)
		return NULL;

	strcpy(fullpath, SFS_DIR);
	strcat(fullpath, path);

	return fullpath;
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
	char *fullpath = NULL;
	time_t now = 0;

	sfs_log(sfs_ctx, SFS_DEBUG, "%s() <<<<<\n", __FUNCTION__);
	// Parameter validation
	if (NULL == path || NULL == stbuf) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters passed. \n",
						__FUNCTION__);
		errno = EINVAL;

		return -1;
	}

	memset(stbuf, 0, sizeof(struct stat));
	fullpath = prepend_mntpath(path);
	sfs_log(sfs_ctx, SFS_DEBUG, "%s: path = %s \n", __FUNCTION__, fullpath);
	ret = lstat(fullpath, stbuf);
	if (ret == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: lstat failed, errno: %d\n",
				__FUNCTION__, errno);
		free(fullpath);

		return -errno;
	} else {
		free(fullpath);
		return 0;
	}
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

	sfs_log(sfs_ctx, SFS_DEBUG, "%s() <<<<<\n", __FUNCTION__);
	// Parameter validation
	if (NULL == path || NULL == buf || size == 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters passed \n",
				__FUNCTION__);
		errno = EINVAL;

		return -1;
	}

	// Get the inode number for the file.
	inodestr = sstack_memcache_read_one(mc, path, strlen(path), &size, sfs_ctx);
	if (NULL == inodestr) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to retrieve the reverse lookup "
					"for path %s.\n", __FUNCTION__, path);
		errno = ENOENT;

		return -1;
	}
	inode_num = atoll((const char *)inodestr);
	// Obtain read lock for the inode
	ret = sfs_rdlock(inode_num);
	if (ret == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to obtain read lock for the "
						"file %s. Try again later \n", __FUNCTION__, path);
		errno = EAGAIN;
		return -1;
	}
	// Get inode from DB
	ret = get_inode(inode_num, &inode, db);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to get inode %lld. Path = %s "
						"error = %d\n", __FUNCTION__, inode_num, path, ret);
		errno = ret;
		sfs_unlock(inode_num);

		return -1;
	}
	sfs_unlock(inode_num);

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
	sstack_free_inode_res(&inode, sfs_ctx);

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
	char *fullpath = NULL;

	sfs_log(sfs_ctx, SFS_DEBUG, "%s() <<<<<\n", __FUNCTION__);
	// Parameter validation
	if (NULL == path || NULL == buf || NULL == fi) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);
		errno = EINVAL;
		return -1;
	}

	sfs_log(sfs_ctx, SFS_DEBUG, "%s: path = %s \n", __FUNCTION__, path);
	syncfs(sfs_ctx->log_fd);

	fullpath = prepend_mntpath(path);
	//sfs_log(sfs_ctx, SFS_DEBUG, "%s: path = %s fullpath = %s\n",
	//		__FUNCTION__, path, fullpath);
	dirpath = opendir(fullpath);
	if (NULL == dirpath) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to open directory %s. "
						"Error = %d \n", __FUNCTION__, path, errno);
		free(fullpath);
		return -1;
	}
	free(fullpath);

	while ((de = readdir(dirpath)) != NULL) {
		struct stat st;

		sfs_log(sfs_ctx, SFS_DEBUG, "%s: name = %s\n", __FUNCTION__,
				de->d_name);
		syncfs(sfs_ctx->log_fd);

		memset(&st, '\0', sizeof(struct stat));
		st.st_ino = de->d_ino;
		st.st_mode = de->d_type << 12;

		if (filler(buf, de->d_name, &st, 0))
			break;
	}

	closedir(dirpath);

	return 0;
}

/*
 * sfs_mkdir - mkdir entry point
 *
 * path - directory path
 * mode - access permissions for the directory
 *
 * This entry point does not result in a call to sfsd as all it needs
 * to do is to create a new inode and issue mknod(2)
 */

int
sfs_mkdir(const char *path, mode_t mode)
{
	sstack_inode_t inode;
	struct stat status;
	int ret = -1;
	char inode_str[MAX_INODE_LEN] = { '\0' };
	char *fullpath = NULL;

	sfs_log(sfs_ctx, SFS_DEBUG, "%s() <<<<<\n", __FUNCTION__);
	// Parameter validation
	if (NULL == path) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified \n",
				__FUNCTION__);
		errno = EINVAL;
		return -1;
	}

	fullpath = prepend_mntpath(path);

	ret = mkdir(fullpath, mode);
	if (ret == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: mkdir of %s  mode %d failed with "
						"error %d \n", __FUNCTION__, fullpath, (int) mode,
						errno);
		free(fullpath);
		return -1;
	}

	// Populate DB with new inode info
	inode.i_num = get_free_inode();
	strcpy(inode.i_name, path);
	inode.i_uid = getuid();
	inode.i_gid = getgid();
	inode.i_mode = mode;
	inode.i_type = DIRECTORY;

	memcpy(&inode.i_atime, &status.st_atime, sizeof(struct timespec));
	memcpy(&inode.i_ctime, &status.st_ctime, sizeof(struct timespec));
	memcpy(&inode.i_mtime, &status.st_mtime, sizeof(struct timespec));
	ret = stat(fullpath, &status);
	if (ret == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: stat failed on %s . Error = %d \n",
						__FUNCTION__, fullpath, errno);
		errno = EACCES;
		rmdir(path);
		free(fullpath);
		return -1;
	}
	free(fullpath);
	inode.i_size = status.st_size;
	inode.i_ondisksize = status.st_size;
	inode.i_links = status.st_nlink;
	// Following are dummy fields
	inode.i_numreplicas = 0;
	inode.i_numextents = 0;
	inode.i_numerasure = 0;
	inode.i_xattrlen = 0;
	inode.i_extent = NULL;
	inode.i_erasure = NULL; // No erasure coding info for now
	inode.i_xattr = NULL; // No extended attributes carried over

	// Store inode
	ret = put_inode(&inode, db, 0);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to store inode #%lld for "
				"path %s. Error %d\n", __FUNCTION__, inode.i_num,
				inode.i_name, errno);
		return -1;
	}
	// Populate memcached for reverse lookup
	sprintf(inode_str, "%lld", inode.i_num);
	ret = sstack_memcache_store(mc, path, inode_str, (strlen(inode_str) + 1),
					sfs_ctx);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to store object into memcached."
				" Key = %s value = %s \n", __FUNCTION__, path, inode_str);
		return -1;
	}

	return 0;
}

/*
 * sfs_unlink - unlink a file
 *
 * path - File to be unlinked
 *
 * Returns 0 upon success and -1 on failure.
 */

int
sfs_unlink(const char *path)
{
	unsigned long long inode_num = 0;
	char *inodestr = NULL;
	sstack_inode_t inode;
	size_t size1 = 0;
	sstack_payload_t *payload = NULL;
	pthread_t thread_id;
	sstack_job_map_t *job_map = NULL;
	int ret = -1;
	int i = 0;
	int j = 0;
	sfsd_t * temp = NULL;
	sfs_job_t *job = NULL;

	sfs_log(sfs_ctx, SFS_DEBUG, "%s() <<<<<\n", __FUNCTION__);
	// Parameter validation
	if (NULL == path) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameter specified \n",
						__FUNCTION__);
		errno = ENOENT;
		return -1;
	}

	ret = unlink(path);
	if (ret == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: unlink of %s failed with error %d \n",
						__FUNCTION__, path, errno);

		return -1;
	}

	// Let all the involved sfsds know about this file's demise

	// Get the inode number for the file.
	inodestr = sstack_memcache_read_one(mc, path, strlen(path), &size1,
			sfs_ctx);
	if (NULL == inodestr) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to retrieve the reverse lookup "
					"for path %s.\n", __FUNCTION__, path);
		errno = ENOENT;

		return -1;
	}
	inode_num = atoll((const char *)inodestr);
	ret = sfs_wrlock(inode_num);
	if (ret == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to obtain write lock for "
						"the file %s\n", __FUNCTION__, path);
		errno = EAGAIN;

		return -1;
	}
	// Get inode from DB
	ret = get_inode(inode_num, &inode, db);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to get inode %lld. Path = %s "
						"error = %d\n", __FUNCTION__, inode_num, path, ret);
		errno = ret;
		sfs_unlock(inode_num);

		return -1;
	}

	// Track new job
	thread_id = pthread_self();
	job_map = create_job_map();
	if (NULL == job_map) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed allocate memory for "
						"job map \n", __FUNCTION__);
		// Free up dynamically allocated fields in inode structure
		sstack_free_inode_res(&inode, sfs_ctx);

		sfs_unlock(inode_num);
		return -1;
	}
	job = sfs_job_init();
	if (NULL == job) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed allocate memory for job.\n",
						__FUNCTION__);
		free_job_map(job_map);
		// Free up dynamically allocated fields in inode structure
		sstack_free_inode_res(&inode, sfs_ctx);
		sfs_unlock(inode_num);
		return -1;
	}

	job->id = get_next_job_id();
	job->job_type = SFSD_IO;
	job->num_clients = inode.i_numclients;
	job->sfsds[0] = inode.i_primary_sfsd->sfsd;

	for (i = 1; i < job->num_clients; i++) {
		job->sfsds[i] = inode.i_sfsds[i].sfsd;
	}
	for (j = 0; j < job->num_clients; j++)
		job->job_status[j] =  JOB_STARTED;

	// Create new payload
	payload = sstack_create_payload(NFS_REMOVE);
	// Populate payload
	payload->hdr.sequence = 0; // Reinitialized by transport
	payload->hdr.payload_len = sizeof(sstack_payload_t);
	payload->hdr.job_id = job->id;
	payload->hdr.priority = job->priority;
	payload->hdr.arg = (uint64_t) job;
	payload->command = NFS_REMOVE;
	// TODO
	// Should we add delimiter to size??
	payload->command_struct.remove_cmd.path_len = (strlen(path) + 1);
	strncpy(payload->command_struct.remove_cmd.path, path,
			payload->command_struct.remove_cmd.path_len);
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
		sfs_unlock(inode_num);

		return -1;
	}
	job_map->job_ids = (sstack_job_id_t *) temp;

	pthread_spin_lock(&job_map->lock);
	job_map->job_ids[job_map->num_jobs - 1] = job->id;
	job_map->num_jobs_left ++;
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
		// Free up dynamically allocated fields in inode structure
		sstack_free_inode_res(&inode, sfs_ctx);
		sfs_unlock(inode_num);

		return -1;
	}
	pthread_spin_lock(&job_map->lock);
	job_map->job_status[job_map->num_jobs - 1] = JOB_STARTED;
	pthread_spin_unlock(&job_map->lock);

	ret = sfs_job2thread_map_insert(thread_id, job->id, job);
	if (ret == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed insert the job context "
							"into RB tree \n", __FUNCTION__);
		free(job_map->job_ids);
		free(job_map->job_status);
		sfs_job_context_remove(thread_id);
		free_job_map(job_map);
		free_payload(sfs_global_cache, payload);
		// Free up dynamically allocated fields in inode structure
		sstack_free_inode_res(&inode, sfs_ctx);
		sfs_unlock(inode_num);

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
		// Free up dynamically allocated fields in inode structure
		sstack_free_inode_res(&inode, sfs_ctx);
		sfs_unlock(inode_num);

		return -1;
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
		// Free up dynamically allocated fields in inode structure
		sstack_free_inode_res(&inode, sfs_ctx);
		sfs_unlock(inode_num);

		return -1;
	}
	// Put the thread to wait
	ret = sfs_wait_for_completion(job_map);

	free(job_map->job_ids);
	free(job_map->job_status);
	free_job_map(job_map);
	free_payload(sfs_global_cache, payload);

	// TODO
	// Handle failure scenarios (HOW??)
	// Success is assumed

	// Delete the inode from DB and free memcached reverse lookup entry
	ret = del_inode(&inode, db);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to delete inode from DB for "
						"file %s \n", __FUNCTION__, path);
		// Free up dynamically allocated fields in inode structure
		sstack_free_inode_res(&inode, sfs_ctx);
		sfs_unlock(inode_num);
		return -1;
	}

	sfs_log(sfs_ctx, SFS_INFO, "%s: Calling sstack_memcache_remove\n",
			__FUNCTION__);
	ret = sstack_memcache_remove(mc, path, sfs_ctx);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to delete reverse lookup "
						"for file %s \n", __FUNCTION__, path);
		// Free up dynamically allocated fields in inode structure
		sstack_free_inode_res(&inode, sfs_ctx);
		sfs_unlock(inode_num);
		return -1;
	}

	// Free up dynamically allocated fields in inode structure
	sstack_free_inode_res(&inode, sfs_ctx);
	sfs_unlock(inode_num);

	return 0;
}


/*
 * sfs_rmdir - remove a directory
 *
 * path - Directory to be removed
 *
 * Returns 0 upon success and -1 on failure.
 */

int
sfs_rmdir(const char *path)
{
	unsigned long long inode_num = 0;
	char *inodestr = NULL;
	sstack_inode_t inode;
	size_t size1 = 0;
	sstack_payload_t *payload = NULL;
	pthread_t thread_id;
	sstack_job_map_t *job_map = NULL;
	int ret = -1;
	int i = 0;
	int j = 0;
	sfsd_t * temp = NULL;
	sfs_job_t *job = NULL;

	sfs_log(sfs_ctx, SFS_DEBUG, "%s() <<<<<\n", __FUNCTION__);
	// Parameter validation
	if (NULL == path) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameter specified \n",
						__FUNCTION__);
		errno = ENOENT;
		return -1;
	}

	ret = rmdir(path);
	if (ret == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: rmdir of %s failed with error %d \n",
						__FUNCTION__, path, errno);

		return -1;
	}

	// Let all the involved sfsds know about this directory's demise

	// Get the inode number for the file.
	inodestr = sstack_memcache_read_one(mc, path, strlen(path), &size1,
			sfs_ctx);
	if (NULL == inodestr) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to retrieve the reverse lookup "
					"for path %s.\n", __FUNCTION__, path);
		errno = ENOENT;

		return -1;
	}
	inode_num = atoll((const char *)inodestr);
	ret = sfs_wrlock(inode_num);
	if (ret == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to obtain write lock for file "
						"%s \n", __FUNCTION__, path);
		errno = EAGAIN;

		return -1;
	}
	// Get inode from DB
	ret = get_inode(inode_num, &inode, db);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to get inode %lld. Path = %s "
						"error = %d\n", __FUNCTION__, inode_num, path, ret);
		errno = ret;

		return -1;
	}

	// Track new job
	thread_id = pthread_self();
	job_map = create_job_map();
	if (NULL == job_map) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed allocate memory for "
						"job map \n", __FUNCTION__);
		// Free up dynamically allocated fields in inode structure
		sstack_free_inode_res(&inode, sfs_ctx);
		sfs_unlock(inode_num);
		return -1;
	}
	job = sfs_job_init();
	if (NULL == job) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed allocate memory for job.\n",
						__FUNCTION__);
		free_job_map(job_map);
		// Free up dynamically allocated fields in inode structure
		sstack_free_inode_res(&inode, sfs_ctx);
		sfs_unlock(inode_num);
		return -1;
	}

	job->id = get_next_job_id();
	job->job_type = SFSD_IO;
	job->num_clients = inode.i_numclients;
	job->sfsds[0] = inode.i_primary_sfsd->sfsd;

	for (i = 1; i < job->num_clients; i++) {
		job->sfsds[i] = inode.i_sfsds[i].sfsd;
	}
	for (j = 0; j < job->num_clients; j++)
		job->job_status[j] =  JOB_STARTED;

	// Create new payload
	payload = sstack_create_payload(NFS_RMDIR);
	// Populate payload
	payload->hdr.sequence = 0; // Reinitialized by transport
	payload->hdr.payload_len = sizeof(sstack_payload_t);
	payload->hdr.job_id = job->id;
	payload->hdr.priority = job->priority;
	payload->hdr.arg = (uint64_t) job;
	payload->command = NFS_RMDIR;
	// TODO
	// Should we add delimiter to size??
	payload->command_struct.remove_cmd.path_len = (strlen(path) + 1);
	payload->command_struct.remove_cmd.path = calloc((strlen(path) + 1), 1);
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
		// Free up dynamically allocated fields in inode structure
		sstack_free_inode_res(&inode, sfs_ctx);
		sfs_unlock(inode_num);

		return -1;
	}
	job_map->job_ids = (sstack_job_id_t *) temp;

	pthread_spin_lock(&job_map->lock);
	job_map->job_ids[job_map->num_jobs - 1] = job->id;
	job_map->num_jobs_left ++;
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
		// Free up dynamically allocated fields in inode structure
		sstack_free_inode_res(&inode, sfs_ctx);
		sfs_unlock(inode_num);

		return -1;
	}
	pthread_spin_lock(&job_map->lock);
	job_map->job_status[job_map->num_jobs - 1] = JOB_STARTED;
	pthread_spin_unlock(&job_map->lock);

	ret = sfs_job2thread_map_insert(thread_id, job->id, job);
	if (ret == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed insert the job context "
							"into RB tree \n", __FUNCTION__);
		free(job_map->job_ids);
		free(job_map->job_status);
		sfs_job_context_remove(thread_id);
		free_job_map(job_map);
		free_payload(sfs_global_cache, payload);
		// Free up dynamically allocated fields in inode structure
		sstack_free_inode_res(&inode, sfs_ctx);
		sfs_unlock(inode_num);

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
		// Free up dynamically allocated fields in inode structure
		sstack_free_inode_res(&inode, sfs_ctx);
		sfs_unlock(inode_num);

		return -1;
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
		// Free up dynamically allocated fields in inode structure
		sstack_free_inode_res(&inode, sfs_ctx);
		sfs_unlock(inode_num);

		return -1;
	}
	// Put the thread to wait
	ret = sfs_wait_for_completion(job_map);
	// TODO
	// Handle failure scenarios (HOW??)
	// Success is assumed

	free(job_map->job_ids);
	free(job_map->job_status);
	free_job_map(job_map);
	free_payload(sfs_global_cache, payload);
	// Free up dynamically allocated fields in inode structure
	sstack_free_inode_res(&inode, sfs_ctx);

	// Delete the inode from DB and free memcached reverse lookup entry
	ret = del_inode(&inode, db);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to delete inode from DB for "
						"file %s \n", __FUNCTION__, path);
		sfs_unlock(inode_num);
		return -1;
	}

	sfs_log(sfs_ctx, SFS_INFO, "%s: Calling sstack_memcache_remove \n",
			__FUNCTION__);

	ret = sstack_memcache_remove(mc, path, sfs_ctx);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to delete reverse lookup "
						"for file %s \n", __FUNCTION__, path);
		sfs_unlock(inode_num);
		return -1;
	}

	sfs_unlock(inode_num);

	return 0;
}

/*
 * sfs_symlink - Create a symbolic link from 'from' to 'to'
 *
 * from - Path of the file to be symbolic linked. Should be non-NULL.
 * to - Symbolic link name. Should be non-NULL.
 *
 * Returns 0 on success and -1 upon failure.
 */

int
sfs_symlink(const char *from, const char *to)
{
	sstack_inode_t inode;
	struct stat status;
	int ret = -1;
	char inode_str[MAX_INODE_LEN] = { '\0' };

	sfs_log(sfs_ctx, SFS_DEBUG, "%s() <<<<<\n", __FUNCTION__);
	// Parameter validation
	if (NULL == from || NULL == to) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);
		errno = EINVAL;

		return -1;
	}

	ret = symlink(from, to);
	if (ret == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Symlink failed with error %d ."
					" From = %s To = %s \n", __FUNCTION__, errno, from, to);

		return -1;
	}

	// Create a new inode representing 'to'
	// Populate DB with new inode info
	inode.i_num = get_free_inode();
	strcpy(inode.i_name, to);
	inode.i_uid = status.st_uid;
	inode.i_gid = status.st_gid;
	inode.i_mode = status.st_mode;
	inode.i_type = SYMLINK;

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
	inode.i_numextents = 0;
	inode.i_numerasure = 0; // No erasure code segments
	inode.i_xattrlen = 0; // No extended attributes
	inode.i_links = status.st_nlink;
	sfs_log(sfs_ctx, SFS_INFO,
		"%s: nlinks for %s are %d\n", __FUNCTION__, to, inode.i_links);
	// Populate the extent
	inode.i_extent = NULL;
	inode.i_erasure = NULL; // No erasure coding info for now
	inode.i_xattr = NULL; // No extended attributes carried over

	// Store inode
	ret = put_inode(&inode, db, 0);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to store inode #%lld for "
				"path %s. Error %d\n", __FUNCTION__, inode.i_num,
				inode.i_name, errno);
		return -1;
	}

	// Populate memcached for reverse lookup
	sprintf(inode_str, "%lld", inode.i_num);
	ret = sstack_memcache_store(mc, to, inode_str, (strlen(inode_str) + 1),
					sfs_ctx);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to store object into memcached."
				" Key = %s value = %s \n", __FUNCTION__, to, inode_str);
		return -1;
	}

	return 0;
}


int
sfs_rename(const char *from, const char *to)
{
	sstack_inode_t 		inode;
	char 				*inode_str = NULL;
	size_t				sz = 0;
	unsigned long long	inode_num = 0;
	int					ret  = 0;

	sfs_log(sfs_ctx, SFS_DEBUG, "%s() <<<<<\n", __FUNCTION__);
	inode_str = sstack_memcache_read_one(mc, from, strlen(from), &sz, sfs_ctx);
	if (NULL == inode_str) {
        sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to retrieve the reverse lookup "
                    "for path %s.\n", __FUNCTION__, from);
        errno = ENOENT;
        return (-1);
    }

	/* Get inode from DB */
	inode_num = atoll((const char *)inode_str);
	ret = sfs_wrlock(inode_num);
	if (ret == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to obtain write lock for file "
						"%s.\n", __FUNCTION__, from);
		errno = EAGAIN;

		return -1;
	}
	ret = get_inode(inode_num, &inode, db);
	if (ret != 0) {
        sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to get inode %lld. Path = %s "
                       "error = %d\n", __FUNCTION__, inode_num, from, ret);
        errno = ret;
		sfs_unlock(inode_num);
        return (-1);
    }

	/* Change inode's path name */
	strcpy(inode.i_name, to);
	/*Put back the inode into DB */
	ret = put_inode(&inode, db, 1);
	if (ret == -1) {
        sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to store inode  %lld in db."
		                        " Inode name = %s \n",
		                        __FUNCTION__, inode.i_num, inode.i_name);
		sfs_unlock(inode_num);
        return (-1);
    }
	// Free up dynamically allocated fields in inode structure
	sstack_free_inode_res(&inode, sfs_ctx);

	sfs_log(sfs_ctx, SFS_INFO, "%s: Calling sstack_memcache_remove \n",
			__FUNCTION__);
	/* Remove the old key and replace with new key in memcached
	 * for reverse lookup */
	ret = sstack_memcache_remove(mc, from, sfs_ctx);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to remove the entry with "
				"old path Key = %s\n", __FUNCTION__, from);
		sfs_unlock(inode_num);
		return (-1);
	}
	ret = sstack_memcache_store(mc, to, inode_str, (strlen(inode_str) + 1),
								sfs_ctx);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to store object into memcached."
	                " Key = %s value = %s \n", __FUNCTION__, to, inode_str);
		sfs_unlock(inode_num);
        return (-1);
    }

	sfs_unlock(inode_num);

	return (0);
}

/*
 * sfs_link - Create a hard link from 'from' to 'to'
 *
 * from - Path of the file to be hard linked. Should be non-NULL.
 * to - Hard link name. Should be non-NULL.
 *
 * Returns 0 on success and -1 upon failure.
 */

int
sfs_link(const char *from, const char *to)
{
	sstack_inode_t inode;
	struct stat status;
	int ret = -1;
	char inode_str[MAX_INODE_LEN] = { '\0' };

	sfs_log(sfs_ctx, SFS_DEBUG, "%s() <<<<<\n", __FUNCTION__);
	// Parameter validation
	if (NULL == from || NULL == to) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);
		errno = EINVAL;

		return -1;
	}

	ret = link(from, to);
	if (ret == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Link failed with error %d ."
					" From = %s To = %s \n", __FUNCTION__, errno, from, to);

		return -1;
	}

	// Create a new inode representing 'to'
	// Populate DB with new inode info
	inode.i_num = get_free_inode();
	strcpy(inode.i_name, to);
	inode.i_uid = status.st_uid;
	inode.i_gid = status.st_gid;
	inode.i_mode = status.st_mode;
	inode.i_type = HARDLINK;

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
	inode.i_numextents = 0;
	inode.i_numerasure = 0; // No erasure code segments
	inode.i_xattrlen = 0; // No extended attributes
	inode.i_links = status.st_nlink;
	sfs_log(sfs_ctx, SFS_INFO,
		"%s: nlinks for %s are %d\n", __FUNCTION__, to, inode.i_links);
	// Populate the extent
	inode.i_extent = NULL;
	inode.i_erasure = NULL; // No erasure coding info for now
	inode.i_xattr = NULL; // No extended attributes carried over

	// Store inode
	ret = put_inode(&inode, db, 0);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to store inode #%lld for "
				"path %s. Error %d\n", __FUNCTION__, inode.i_num,
				inode.i_name, errno);
		return -1;
	}

	// Populate memcached for reverse lookup
	sprintf(inode_str, "%lld", inode.i_num);
	ret = sstack_memcache_store(mc, to, inode_str, (strlen(inode_str) + 1),
					sfs_ctx);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to store object into memcached."
				" Key = %s value = %s \n", __FUNCTION__, to, inode_str);
		return -1;
	}

	return 0;
}

/*
 * sfs_chmod - Change access permissions of the file
 *
 * path - File whose permission needs to be changed
 * mode - new file permissions
 *
 * Returns 0 on success and -1 upon failure.
 */

int
sfs_chmod(const char *path, mode_t mode)
{
	unsigned long long inode_num = 0;
	char *inodestr = NULL;
	sstack_inode_t inode;
	size_t size1 = 0;
	int ret = -1;
	char *fullpath = NULL;

	sfs_log(sfs_ctx, SFS_DEBUG, "%s() <<<<<\n", __FUNCTION__);
	// Parameter validation
	if (NULL == path) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);
		errno = ENOENT;

		return -1;
	}

	fullpath = prepend_mntpath(path);
	ret = chmod(fullpath, mode);
	if (ret == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: chmod for %s failed with error %d\n",
						__FUNCTION__, fullpath, errno);
		free(fullpath);

		return -1;
	}

	free(fullpath);
	// Update inode with the new mode
	// Get the inode number for the file.
	inodestr = sstack_memcache_read_one(mc, path, strlen(path), &size1,
			sfs_ctx);
	if (NULL == inodestr) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to retrieve the reverse lookup "
					"for path %s.\n", __FUNCTION__, path);
		errno = ENOENT;

		return -1;
	}
	inode_num = atoll((const char *)inodestr);
	ret = sfs_wrlock(inode_num);
	if (ret == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to obtain write lock for file "
						"%s\n", __FUNCTION__, path);
		errno = EAGAIN;

		return -1;
	}

	// Get inode from DB
	ret = get_inode(inode_num, &inode, db);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to get inode %lld. Path = %s "
						"error = %d\n", __FUNCTION__, inode_num, path, ret);
		errno = ret;
		sfs_unlock(inode_num);

		return -1;
	}

	inode.i_mode = mode;

	// Store the inode back to DB
	ret = put_inode(&inode, db, 1);
	if (ret == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to store inode  %lld in db."
						" Inode name = %s \n",
						__FUNCTION__, inode.i_num, inode.i_name);
		// Free up dynamically allocated fields in inode structure
		sstack_free_inode_res(&inode, sfs_ctx);
		sfs_unlock(inode_num);
		return -1;
	}

	// Free up dynamically allocated fields in inode structure
	sstack_free_inode_res(&inode, sfs_ctx);
	sfs_unlock(inode_num);

	return 0;
}


/*
 * sfs_chown - Change ownership of the file
 *
 * path - File whose ownsership needs to be changed
 * uid - User id
 * gid - Group id
 *
 * Returns 0 on success and -1 upon failure.
 */

int
sfs_chown(const char *path, uid_t uid, gid_t gid)
{
	unsigned long long inode_num = 0;
	char *inodestr = NULL;
	sstack_inode_t inode;
	size_t size1 = 0;
	int ret = -1;

	sfs_log(sfs_ctx, SFS_DEBUG, "%s() <<<<<\n", __FUNCTION__);
	// Parameter validation
	if (NULL == path) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);
		errno = ENOENT;

		return -1;
	}

	ret = chown(path, uid, gid);
	if (ret == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: chown for %s failed with error %d\n",
						__FUNCTION__, errno);
		return -1;
	}

	// Update inode with the new mode
	// Get the inode number for the file.
	inodestr = sstack_memcache_read_one(mc, path, strlen(path), &size1,
			sfs_ctx);
	if (NULL == inodestr) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to retrieve the reverse lookup "
					"for path %s.\n", __FUNCTION__, path);
		errno = ENOENT;

		return -1;
	}
	inode_num = atoll((const char *)inodestr);
	ret = sfs_wrlock(inode_num);
	if (ret == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to obtain write lock for file "
						"%s\n", __FUNCTION__, path);
		errno = EAGAIN;

		return -1;
	}
	// Get inode from DB
	ret = get_inode(inode_num, &inode, db);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to get inode %lld. Path = %s "
						"error = %d\n", __FUNCTION__, inode_num, path, ret);
		errno = ret;
		sfs_unlock(inode_num);

		return -1;
	}

	inode.i_uid = uid;
	inode.i_gid = gid;

	// Store the inode back to DB
	ret = put_inode(&inode, db, 1);
	if (ret == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to store inode  %lld in db."
						" Inode name = %s \n",
						__FUNCTION__, inode.i_num, inode.i_name);
		// Free up dynamically allocated fields in inode structure
		sstack_free_inode_res(&inode, sfs_ctx);
		sfs_unlock(inode_num);
		return -1;
	}

	// Free up dynamically allocated fields in inode structure
	sstack_free_inode_res(&inode, sfs_ctx);
	sfs_unlock(inode_num);

	return 0;
}

/*
 * sfs_truncate - truncate entry point of sfs
 *
 * path - path name of the file
 * size - size of the file after the successful operation
 *
 * Retruns 0 on success and -1 on failure.
 */
int
sfs_truncate(const char *path, off_t size)
{
	sstack_inode_t inode;
	int ret = -1;
	char *inodestr = NULL;
	unsigned long long inode_num = 0;
	size_t size1 = 0;

	sfs_log(sfs_ctx, SFS_DEBUG, "%s() <<<<<\n", __FUNCTION__);
	// Parameter validation
	if (NULL == path)  {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameter specified \n",
						__FUNCTION__);
		errno = ENOENT;
		return -1;
	}

	ret = truncate(path, size);
	if (ret == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: truncate failed for file %s with "
						"error %d \n", __FUNCTION__, path, errno);
		return -1;
	}

	// Get the inode number for the file.
	inodestr = sstack_memcache_read_one(mc, path, strlen(path), &size1,
			sfs_ctx);
	if (NULL == inodestr) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to retrieve the reverse lookup "
					"for path %s.\n", __FUNCTION__, path);
		errno = ENOENT;

		return -1;
	}
	inode_num = atoll((const char *)inodestr);
	ret = sfs_wrlock(inode_num);
	if (ret == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to obtain write lock for file "
						"%s\n", __FUNCTION__, path);
		errno = EAGAIN;

		return -1;
	}

	// Get inode from DB
	ret = get_inode(inode_num, &inode, db);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to get inode %lld. Path = %s "
						"error = %d\n", __FUNCTION__, inode_num, path, ret);
		errno = ret;
		sfs_unlock(inode_num);

		return -1;
	}

	// Update inode size
	inode.i_size = size;
	// TODO
	// Remove the extent information and sfsds to remove the extent files
	// There is no NFSv3 operation for truncate. We need to device a new
	// operation

	// Store inode
	ret = put_inode(&inode, db, 1);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to store inode #%lld for "
				"path %s. Error %d\n", __FUNCTION__, inode.i_num,
				inode.i_name, errno);
		sstack_free_inode_res(&inode, sfs_ctx);
		sfs_unlock(inode_num);
		return -1;
	}

	// Free up dynamically allocated fields in inode structure
	sstack_free_inode_res(&inode, sfs_ctx);
	sfs_unlock(inode_num);

	return 0;
}


/*
 * sfs_open - Open entry point
 *
 * path - Full path name of the file. Should be non-NULL
 * fi - FUSE represention of the file. Should be non-NULL
 *
 * Returns 0 on success and -1 upon failure.
 */

int
sfs_open(const char *path, struct fuse_file_info *fi)
{
	sstack_inode_t *inode = NULL;
	struct stat status;
	int ret = -1;
	char *inodestr = NULL;
	int fd = -1;
	size_t size;
	char inode_str[MAX_INODE_LEN] = { '\0' };
	char *fullpath = NULL;

	sfs_log(sfs_ctx, SFS_DEBUG, "%s() <<<<<\n", __FUNCTION__);
	// Parameter validation
	if (NULL == path || NULL == fi) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified \n",
					__FUNCTION__);
		errno = ENOENT;
		return -1;
	}

	fullpath = prepend_mntpath(path);

	fd = open(fullpath, fi->flags);
	if (fd  == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Open on %s failed with error %d \n",
						__FUNCTION__, path, errno);
		free(fullpath);

		return -1;
	}

	sfs_log(sfs_ctx, SFS_INFO, "%s: Open of %s succeeded \n", __FUNCTION__,
			path);

	// Check if the file alreday exists in reverse lookup
	inodestr = sstack_memcache_read_one(mc, path, strlen(path), &size, sfs_ctx);
	sfs_log(sfs_ctx, SFS_DEBUG, "%s: inodestr = %s \n", __FUNCTION__, inodestr);
	syncfs(sfs_ctx->log_fd);
	free(fullpath);
	// Update file handle to FUSE
	fi->fh = fd;

	// Populate DB with new inode info
	// Check if inode already exists
	// If so, only update the access time
	inode = sstack_create_inode();
	if (inode == NULL) {
		sfs_log(sfs_ctx, SFS_ERR, "%s() - inode allcation failed\n",
				__FUNCTION__);
		return -ENOMEM;
	}
	ret = get_inode(atoll(inodestr), inode, db);
	if (ret != 0) {
		inode->i_num = get_free_inode();
		strcpy(inode->i_name, path);
		inode->i_uid = status.st_uid;
		inode->i_gid = status.st_gid;
		inode->i_mode = status.st_mode;
		switch (status.st_mode & S_IFMT) {
			case S_IFDIR:
				inode->i_type = DIRECTORY;
				break;
			case S_IFREG:
				inode->i_type = REGFILE;
				break;
			case S_IFLNK:
				inode->i_type = SYMLINK;
				break;
			case S_IFBLK:
				inode->i_type = BLOCKDEV;
				break;
			case S_IFCHR:
				inode->i_type = CHARDEV;
				break;
			case S_IFIFO:
				inode->i_type = FIFO;
				break;
			case S_IFSOCK:
				inode->i_type = SOCKET_TYPE;
				break;
			default:
				inode->i_type = UNKNOWN;
				break;
		}

		// Make sure we are not looping
		// TBD
		memcpy(&inode->i_atime, &status.st_atime, sizeof(struct timespec));
		memcpy(&inode->i_ctime, &status.st_ctime, sizeof(struct timespec));
		memcpy(&inode->i_mtime, &status.st_mtime, sizeof(struct timespec));
		inode->i_size = status.st_size;
		inode->i_ondisksize = (status.st_blocks * 512);
		inode->i_numreplicas = 1; // For now, single copy
		// Since the file already exists, done't split it now. Split it when
		// next write arrives
		inode->i_numextents = 1;
		inode->i_numerasure = 0; // No erasure code segments
		inode->i_xattrlen = 0; // No extended attributes
		inode->i_links = status.st_nlink;
		sfs_log(sfs_ctx, SFS_INFO,
				"%s: nlinks for %s are %d\n", __FUNCTION__, path, inode->i_links);
		// Populate the extent

		// Store inode
		ret = put_inode(inode, db, 0);
		if (ret != 0) {
			sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to store inode #%lld for "
					"path %s. Error %d\n", __FUNCTION__, inode->i_num,
					inode->i_name, errno);
			return -1;
		}
	} else {
		// File already exists.
		// Update access time and update inode
		memcpy(&inode->i_atime, &status.st_atime, sizeof(struct timespec));
		ret = put_inode(inode, db, 1);
		sstack_dump_inode(inode, sfs_ctx);
		if (ret != 0) {
			sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to store inode #%lld for "
					"path %s. Error %d\n", __FUNCTION__, inode->i_num,
					inode->i_name, errno);
			return -1;
		}
	}

	sfs_log(sfs_ctx, SFS_INFO, "%s: put_inode succeeded \n", __FUNCTION__);


	// Populate memcached for reverse lookup
	sprintf(inode_str, "%lld", inode->i_num);
	ret = sstack_memcache_store(mc, path, inode_str, (strlen(inode_str) + 1),
					sfs_ctx);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to store object into memcached."
				" Key = %s value = %s \n", __FUNCTION__, path, inode_str);
		return -1;
	}

	return 0;
}


int
sfs_read(const char *path, char *buf, size_t size, off_t offset,
	struct fuse_file_info *fi)
{
	unsigned long long inode_num = 0;
	char *inodestr = NULL;
	sstack_inode_t *inode = NULL;
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
//	sfsd_list_t *sfsds = NULL;

	sfs_log(sfs_ctx, SFS_DEBUG, "%s() <<<<<\n", __FUNCTION__);
	// Paramater validation
	if (NULL == path || NULL == buf || size < 0 || offset < 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters \n",
				__FUNCTION__);
		errno = EINVAL;

		return -1;
	}

	sfs_log(sfs_ctx, SFS_DEBUG, "%s: path = %s size = %d offset %d \n",
			__FUNCTION__, path, size, offset);
	syncfs(sfs_ctx->log_fd);

	// Get the inode number for the file.
	inodestr = sstack_memcache_read_one(mc, path, strlen(path), &size1,
			sfs_ctx);
	if (NULL == inodestr) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to retrieve the reverse lookup "
					"for path %s.\n", __FUNCTION__, path);
		errno = ENOENT;

		return -1;
	}

	inode_num = atoll((const char *)inodestr);
	ret = sfs_rdlock(inode_num);
	if (ret == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to obtain read lock for file "
						"%s\n", __FUNCTION__, path);
		errno = EAGAIN;

		return -1;
	}
	if ((inode = sstack_create_inode()) == NULL) {
		sfs_log(sfs_ctx, SFS_ERR, "%s() - Failed getting inode\n",
				__FUNCTION__);
		return -ENOMEM;
	}
	// Get inode from DB
	ret = get_inode(inode_num, inode, db);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to get inode %lld. Path = %s "
						"error = %d\n", __FUNCTION__, inode_num, path, ret);
		errno = ret;
		sfs_unlock(inode_num);

		return -1;
	}
	sstack_dump_inode(inode, sfs_ctx);
	sstack_dump_extent(inode->i_extent, sfs_ctx);

	sfs_log(sfs_ctx, SFS_DEBUG, "%s: Inode size %d size %d offset %d \n",
			__FUNCTION__, inode->i_size, size, offset);
	syncfs(sfs_ctx->log_fd);
#if 0
	// Parameter validation; AGAIN :-)
	if (inode->i_size < size || (offset + size ) > inode->i_size ) {
		// Appl asking for file offset greater tha real size
		// Return error
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid offset/size specified \n",
				__FUNCTION__);
		errno = EINVAL;
		// Free up dynamically allocated fields in inode structure
		sstack_free_inode_res(&inode, sfs_ctx);
		sfs_unlock(inode_num);

		return -1;
	}
#endif

	relative_offset = offset;
#if 0
	// Get the sfsd information from IDP
	sfsds = sfs_idp_get_sfsd_list(&inode, sfsd_pool, sfs_ctx);
	if (NULL == sfsds) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: sfs_idp_get_sfsd_list failed \n",
				__FUNCTION__);
		// Free up dynamically allocated fields in inode structure
		sstack_free_inode_res(&inode, sfs_ctx);
		sfs_unlock(inode_num);
		return -1;
	}
#endif
	// Check for metadata validity
	if (inode->i_numclients == 0 && NULL == inode->i_primary_sfsd) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Inode metadata corrupt for file %s \n",
						__FUNCTION__, path);
		errno = EIO;
		// Free up dynamically allocated fields in inode structure
		sstack_free_inode_res(inode, sfs_ctx);
		sfs_unlock(inode_num);

		return -1;
	}


	thread_id = pthread_self();

	// Get the extent covering the request
	for(i = 0; i < inode->i_numextents; i++) {
		extent = &inode->i_extent[i];
		if (extent->e_offset <= relative_offset &&
				(extent->e_offset + extent->e_size >= relative_offset)) {
			// Found the initial extent
			break;
		}
		extent ++;
	}
	sstack_dump_extent(extent, sfs_ctx);

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
		// Free up dynamically allocated fields in inode structure
		sstack_free_inode_res(inode, sfs_ctx);
		sfs_unlock(inode_num);
		return -1;
	}
	// job_map->command = NFS_READ;

#if 0
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
#endif

	if (size > inode->i_size) {
		bytes_to_read = inode->i_size;
	} else {
		bytes_to_read = size;
	}

	while (bytes_to_read) {
		sfs_job_t *job = NULL;
		size_t read_size = 0;
		char *temp = NULL;

		// Submit job
		job = sfs_job_init();
		if (NULL == job) {
			sfs_log(sfs_ctx, SFS_ERR, "%s: Failed allocate memory for job.\n",
					__FUNCTION__);
			free_job_map(job_map);
			// Free up dynamically allocated fields in inode structure
			sstack_free_inode_res(inode, sfs_ctx);
			sfs_unlock(inode_num);
			return -1;
		}

		job->id = get_next_job_id();
		job->job_type = SFSD_IO;
		job->num_clients = 1;
		job->sfsds[0] = inode->i_primary_sfsd->sfsd;
		job->job_status[0] = JOB_STARTED;
#if 0
		for (j = 0; j < inode->i_numclients; j++)
			job->job_status[j] =  JOB_STARTED;
#endif


		//job->priority = policy->pe_attr.a_qoslevel;
		job->priority = QOS_HIGH;
		// Create new payload
		payload = sstack_create_payload(NFS_READ);
		// Populate payload
		payload->hdr.sequence = 0; // Reinitialized by transport
		payload->hdr.payload_len = sizeof(sstack_payload_t);
		payload->hdr.job_id = job->id;
		payload->hdr.priority = job->priority;
		payload->hdr.arg = (uint64_t) job;
		payload->command = NFS_READ;
		payload->command_struct.read_cmd.inode_no = inode->i_num;
		payload->command_struct.read_cmd.offset = offset;
		if ((offset + size) > (extent->e_offset + extent->e_size))
			read_size = (extent->e_offset + extent->e_size) - offset;
		payload->command_struct.read_cmd.count = read_size;
		payload->command_struct.read_cmd.read_ecode = 0;
		/*memcpy((void *) &payload->command_struct.read_cmd.pe, (void *)
						policy, sizeof(struct policy_entry));*/
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
			// Free up dynamically allocated fields in inode structure
			sstack_free_inode_res(inode, sfs_ctx);
			sfs_unlock(inode_num);

			return -1;
		}
		job_map->job_ids = (sstack_job_id_t *) temp;

		pthread_spin_lock(&job_map->lock);
		job_map->job_ids[job_map->num_jobs - 1] = job->id;
		job_map->num_jobs_left ++;
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
			// Free up dynamically allocated fields in inode structure
			sstack_free_inode_res(inode, sfs_ctx);
			sfs_unlock(inode_num);

			return -1;
		}
		job_map->job_status = (sstack_job_status_t *)temp;
		pthread_spin_lock(&job_map->lock);
		job_map->job_status[job_map->num_jobs - 1] = JOB_STARTED;
		pthread_spin_unlock(&job_map->lock);

		ret = sfs_job2thread_map_insert(thread_id, job->id, job);
		if (ret == -1) {
			sfs_log(sfs_ctx, SFS_ERR, "%s: Failed insert the job context "
							"into RB tree \n", __FUNCTION__);
			free(job_map->job_ids);
			free(job_map->job_status);
			sfs_job_context_remove(thread_id);
			free_job_map(job_map);
			free_payload(sfs_global_cache, payload);
			// Free up dynamically allocated fields in inode structure
			sstack_free_inode_res(inode, sfs_ctx);
			sfs_unlock(inode_num);

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
			// Free up dynamically allocated fields in inode structure
			sstack_free_inode_res(inode, sfs_ctx);
			sfs_unlock(inode_num);

			return -1;
		}

		bytes_issued += read_size;
#if 0
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
#endif
		bytes_to_read -= bytes_issued;
		sfs_log(sfs_ctx, SFS_DEBUG, "%s() - %d - bytes_to_read: %d\n",
				__FUNCTION__, __LINE__,bytes_to_read);
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
		// Free up dynamically allocated fields in inode structure
		sstack_free_inode_res(inode, sfs_ctx);
		sfs_unlock(inode_num);

		return -1;
	}

	ret = sfs_wait_for_completion(job_map);

	ret = sfs_send_read_status(job_map, buf, size);

	sfs_job_context_remove(thread_id);
	free(job_map->job_ids);
	free(job_map->job_status);
	free_job_map(job_map);
	// Free up dynamically allocated fields in inode structure
	sstack_free_inode_res(inode, sfs_ctx);
	sfs_unlock(inode_num);

	return (ret);
}

extern sfsd_t accept_sfsd;

int
sfs_write(const char *path, const char *buf, size_t size, off_t offset,
	struct fuse_file_info *fi)
{
	unsigned long long inode_num = 0;
	char *inodestr = NULL;
	sstack_inode_t *inode = NULL;
	size_t size1 = 0;
	sstack_extent_t *extent = NULL;
	struct stat st = { 0x0 };
	int ret = -1;
	sstack_payload_t *payload = NULL;
	pthread_t thread_id;
	sstack_job_map_t *job_map = NULL;
	off_t relative_offset = 0;
	int bytes_to_write = 0;
	policy_entry_t *policy = NULL;
	size_t bytes_issued = 0;
	int i = 0;
	char *fullpath = NULL;

	sfs_log(sfs_ctx, SFS_DEBUG, "%s() <<<<<\n", __FUNCTION__);
	// Paramater validation
	if (NULL == path || NULL == buf || size < 0 || offset < 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters \n",
				__FUNCTION__);
		errno = EINVAL;

		return -1;
	}

	// Get the inode number for the file.
	inodestr = sstack_memcache_read_one(mc, path, strlen(path), &size1,
			sfs_ctx);
	if (NULL == inodestr) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to retrieve the reverse lookup "
					"for path %s.\n", __FUNCTION__, path);
		errno = ENOENT;

		return -1;
	}
	inode_num = atoll((const char *)inodestr);
	ret = sfs_wrlock(inode_num);
	if (ret == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to obtain write lock for file "
						"%s\n", __FUNCTION__, path);
		errno = EAGAIN;

		return -1;
	}
	// Get inode from DB
	inode = sstack_create_inode();
	if (inode == NULL) {
		sfs_log(sfs_ctx, SFS_DEBUG, "%s() - Unable to allocate inode",
				__FUNCTION__);
		return -ENOMEM;
	}

	ret = get_inode(inode_num, inode, db);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to get inode %lld. Path = %s "
						"error = %d\n", __FUNCTION__, inode_num, path, ret);
		errno = ret;
		sfs_unlock(inode_num);

		return -1;
	}

	// Check for validity of metadata
	fullpath = prepend_mntpath(path);
	ret = stat(fullpath, &st);
	if (ret == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: stat failed for file %s. Error = %d \n",
						__FUNCTION__, fullpath, errno);

		sfs_unlock(inode_num);
		free(fullpath);

		return -1;
	}
	free(fullpath);

	if (st.st_size > 0 &&
				(inode->i_numclients == 0 || NULL == inode->i_sfsds)) {
		// It is not feasible that file has some contents but no sfsds are
		// assigned to it.
		sfs_log(sfs_ctx, SFS_ERR, "%s: Metadata corrupt for file %s \n",
						__FUNCTION__, path);

		errno = EIO;
		sfs_unlock(inode_num);

		return -1;
	}

	// If first write for the file, get the sfsd list for the file.
#if 0
	if (st.st_size == 0) {
		sfsd_list_t *sfsds = NULL;
		sstack_sfsd_info_t *temp = NULL;
		int i = 0;


		sfs_log(sfs_ctx, SFS_DEBUG, "%s() - %d\n", __FUNCTION__, __LINE__);
		sfsds = sfs_idp_get_sfsd_list(inode, sfsd_pool, sfs_ctx);
		if (NULL == sfsds) {
			sfs_log(sfs_ctx, SFS_ERR, "%s: sfs_idp_get_sfsd_list failed \n",
							__FUNCTION__);
			// Free up dynamically allocated fields in inode structure
			sstack_free_inode_res(inode, sfs_ctx);
			sfs_unlock(inode_num);
			errno = EIO;

			return -1;
		}
		sfs_log(sfs_ctx, SFS_DEBUG, "%s() - %d\n", __FUNCTION__, __LINE__);
		// Populate inode with new information
		inode->i_numclients = sfsds->num_sfsds;
		temp = (sstack_sfsd_info_t *) malloc(sizeof(sstack_sfsd_info_t));
		if (NULL == temp) {
			sfs_log(sfs_ctx, SFS_ERR, "%s: Out of memory. File %s \n",
							__FUNCTION__, path);

			errno = EIO;
			sstack_free_inode_res(inode, sfs_ctx);
			sfs_unlock(inode_num);
			free(sfsds);

			return -1;
		}
		sstack_sfsd_info_t temp;
		memcpy((void *) &temp->transport, (void *)sfsds[0].sfsds->transport,
						sizeof(sstack_transport_t));
		inode->i_primary_sfsd = temp;
		// Populate rest of the fields
		inode->i_sfsds = (sstack_sfsd_info_t *)
				malloc((inode->i_numclients -1) * sizeof(sstack_sfsd_info_t));
		if (NULL == inode->i_sfsds) {
			sfs_log(sfs_ctx, SFS_ERR, "%s: Out of memory. File %s \n",
							__FUNCTION__, path);
			errno = EIO;
			sstack_free_inode_res(inode, sfs_ctx);
			sfs_unlock(inode_num);
			free(sfsds);

			return -1;
		}

		for (i = 1; i < inode->i_numclients; i++) {
			memcpy((void *) &inode->i_sfsds[i].transport, (void *)
					sfsds[i].sfsds->transport, sizeof(sstack_transport_t));
		}
		free(sfsds);
		// TODO
		// Store the inode in DB to avoid losing this information
	}
#endif

	//FIXME: Hardcoding sfsds for now
	inode->i_numclients = 1;
	inode->i_sfsds = malloc(sizeof(sstack_sfsd_info_t));
	inode->i_sfsds->sfsd = &accept_sfsd;
	/*memcpy(&inode->i_sfsds[0].transport, &accept_sfsd.transport,
			sizeof(sstack_transport_t));*/
	inode->i_primary_sfsd = inode->i_sfsds;
	sfs_log(sfs_ctx, SFS_DEBUG, "%s() - %d, primary sfsd: %p %p\n",
			__FUNCTION__, __LINE__, inode->i_primary_sfsd, inode->i_sfsds);
	sstack_dump_inode(inode, sfs_ctx);

	if (put_inode(inode, db, 1) != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s() - put_inode failed\n",
				__FUNCTION__);
		return -1;
	}
#if 0
	//REMOVE
	memset(inode, 0, sizeof(*inode));
	get_inode(3, inode, db);
	sfs_log(sfs_ctx, SFS_DEBUG, "%s() - read after update\n",
			__FUNCTION__);
	sstack_dump_inode(inode, sfs_ctx);
#endif
#if 1
	relative_offset = offset;
	thread_id = pthread_self();

	// Get the extent covering the request
	for(i = 0; i < inode->i_numextents; i++) {
		extent = inode->i_extent;
		if (extent->e_offset <= relative_offset &&
				(extent->e_offset + extent->e_size >= relative_offset)) {
			// Found the initial extent
			break;
		}
		extent ++;
	}

	bytes_to_write = size;
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
		// Free up dynamically allocated fields in inode structure
		sstack_free_inode_res(inode, sfs_ctx);
		sfs_unlock(inode_num);
		return -1;
	}

	for (i = 0; i < inode->i_numclients; i++)
		job_map->op_status[i] = JOB_STARTED;

	/*
	 * Note: Don't free policy. It is just a pointer to original DS
	 * and no copies are made by get_policy.
	 */
	policy = get_policy(path);
#if 0
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
#endif
	while (bytes_to_write) {
		sfs_job_t *job = NULL;
		int j = -1;
		size_t write_size = 0;
		char *temp = NULL;

		// Submit job
		job = sfs_job_init();
		if (NULL == job) {
			sfs_log(sfs_ctx, SFS_ERR, "%s: Failed allocate memory for job.\n",
					__FUNCTION__);
			free_job_map(job_map);
			// Free up dynamically allocated fields in inode structure
			sstack_free_inode_res(inode, sfs_ctx);
			sfs_unlock(inode_num);
			return -1;
		}

		job->id = get_next_job_id();
		job->job_type = SFSD_IO;
		job->num_clients = inode->i_numclients;
		//FIXME:
		//job->sfsds[0] = inode.i_primary_sfsd->sfsd;
		//for (j = 0; j < (inode.i_numclients - 1); j++)
		//	job->sfsds[j + 1] = inode.i_sfsds[j].sfsd;
		//for (j = 0; j < inode.i_numclients; j++)
		//	job->job_status[j] =  JOB_STARTED;
		job->sfsds[0] = &accept_sfsd;
		job->job_status[0] =  JOB_STARTED;
		job->priority = QOS_HIGH;
		//job->priority = policy->pe_attr.a_qoslevel;
		// Create new payload
		payload = sstack_create_payload(NFS_WRITE);
		// Populate payload
		payload->hdr.sequence = 0; // Reinitialized by transport
		payload->hdr.payload_len = sizeof(sstack_payload_t);
		payload->hdr.job_id = job->id;
		payload->hdr.priority = job->priority;
		payload->hdr.arg = (uint64_t) job;
		payload->command = NFS_WRITE;
		payload->command_struct.write_cmd.inode_no = inode->i_num;
		payload->command_struct.write_cmd.offset = offset;
		//TODO: Extent is unallocated here. Take care for the fisrt
		//extent

		/* Could be first extent. take care here */
		if (extent) {
			if ((offset + size) > (extent->e_offset + extent->e_size))
				write_size = (extent->e_offset + extent->e_size) - offset;
		} else {
			write_size = size;
		}
		payload->command_struct.write_cmd.count = write_size;
		payload->command_struct.write_cmd.data.data_len = write_size;
		payload->command_struct.write_cmd.data.data_buf =
				(unsigned char *) (buf + bytes_issued);
		sfs_log(sfs_ctx, SFS_DEBUG, "Data1: %s\n",
				payload->command_struct.write_cmd.data.data_buf);
		//memcpy((void *) &payload->command_struct.read_cmd.pe, (void *)
		//				policy, sizeof(struct policy_entry));
		job->payload_len = sizeof(sstack_payload_t);
		job->payload = payload;
		// Add this job to job_map
		job_map->num_jobs ++;
		sfs_log(sfs_ctx, SFS_DEBUG, "%s() - %d num jobs: %d\n",
				__FUNCTION__, __LINE__, job_map->num_jobs);
		temp = realloc(job_map->job_ids, job_map->num_jobs *
						sizeof(sstack_job_id_t));
		if (NULL == temp) {
			sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to allocate memory for "
							"job_ids \n", __FUNCTION__);
			if (job_map->job_ids)
				free(job_map->job_ids);
			free_job_map(job_map);
			free_payload(sfs_global_cache, payload);
			// Free up dynamically allocated fields in inode structure
			sstack_free_inode_res(inode, sfs_ctx);
			sfs_unlock(inode_num);

			return -1;
		}
		job_map->job_ids = (sstack_job_id_t *) temp;

		pthread_spin_lock(&job_map->lock);
		job_map->job_ids[job_map->num_jobs - 1] = job->id;
		job_map->num_jobs_left ++;
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
			// Free up dynamically allocated fields in inode structure
			sstack_free_inode_res(inode, sfs_ctx);
			sfs_unlock(inode_num);

			return -1;
		}
		job_map->job_status = (sstack_job_status_t *)temp;
		pthread_spin_lock(&job_map->lock);
		job_map->job_status[job_map->num_jobs - 1] = JOB_STARTED;
		pthread_spin_unlock(&job_map->lock);

		ret = sfs_job2thread_map_insert(thread_id, job->id, job);
		if (ret == -1) {
			sfs_log(sfs_ctx, SFS_ERR, "%s: Failed insert the job context "
							"into RB tree \n", __FUNCTION__);
			free(job_map->job_ids);
			free(job_map->job_status);
			sfs_job_context_remove(thread_id);
			free_job_map(job_map);
			free_payload(sfs_global_cache, payload);
			// Free up dynamically allocated fields in inode structure
			sstack_free_inode_res(inode, sfs_ctx);
			sfs_unlock(inode_num);

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
			// Free up dynamically allocated fields in inode structure
			sstack_free_inode_res(inode, sfs_ctx);
			sfs_unlock(inode_num);

			return -1;
		}

		bytes_issued += write_size;

		sfs_log(sfs_ctx, SFS_DEBUG, "%s() - %d bytes issued: %d\n",
				__FUNCTION__, __LINE__, bytes_issued);
		if (extent) {
			// Check whether we are done with original request
			if ((offset + size) > (extent->e_offset + extent->e_size)) {
				// Request spans multiple extents
				extent ++;
				// TODO
				// Check for sparse file condition
				relative_offset = extent->e_offset;
				bytes_to_write = size - bytes_issued;
			} else {
				bytes_to_write = size - bytes_issued;
			}
		}
		bytes_to_write = size - bytes_issued;
		sfs_log(sfs_ctx, SFS_DEBUG, "%s() - %d bytes to write: %d\n",
				__FUNCTION__, __LINE__, bytes_to_write);
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
		// Free up dynamically allocated fields in inode structure
		sstack_free_inode_res(inode, sfs_ctx);
		sfs_unlock(inode_num);

		return -1;
	}

	ret = sfs_wait_for_completion(job_map);

	// TODO
	// Handle write status and erasure code

	ret = sfs_send_write_status(job_map, *inode, offset, size);

	sfs_job_context_remove(thread_id);
	free(job_map->job_ids);
	free(job_map->job_status);
	free_job_map(job_map);
	// Free up dynamically allocated fields in inode structure
	sstack_free_inode_res(inode, sfs_ctx);
	sfs_unlock(inode_num);
	memset(inode, 0, sizeof(*inode));
	get_inode(3, inode, db);
	sstack_dump_inode(inode, sfs_ctx);
	sstack_free_inode(inode);

	sfs_log(sfs_ctx, SFS_DEBUG, "%s() - %d ret %d\n",
			__FUNCTION__, __LINE__, ret);
	return (ret);
#endif
	return 0;
}

/*
 * sfs_statfs - Provide file system statistics
 */

int
sfs_statfs(const char *path, struct statvfs *buf)
{

	sfs_log(sfs_ctx, SFS_DEBUG, "%s() <<<<<\n", __FUNCTION__);
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

	sfs_log(sfs_ctx, SFS_DEBUG, "%s() <<<<<\n", __FUNCTION__);
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
#if 0
	sfs_log(sfs_ctx, SFS_INFO, "%s: Calling sstack_memcache_remove \n",
			__FUNCTION__);
	// Remove the reverse mapping for the path
	res = sstack_memcache_remove(mc, path, sfs_ctx);
	if (res != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Reverse lookup for path %s failed. "
						"Return value = %d\n", __FUNCTION__, path, res);
	}
#endif
	sfs_log(sfs_ctx, SFS_DEBUG, "%s() - Release done\n", __FUNCTION__);
	return 0;
}


int
sfs_fsync(const char *path, int isdatasync, struct fuse_file_info *fi)
{
	sfs_log(sfs_ctx, SFS_DEBUG, "%s() <<<<<\n", __FUNCTION__);

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
	ret = put_inode(inode, db, 1);
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
	ret = put_inode(inode, db, 1);
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

	sfs_log(sfs_ctx, SFS_DEBUG, "%s() <<<<<\n", __FUNCTION__);
	// Parameter validation
	if (NULL == path || NULL == name || NULL == value || size == 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters passed. \n",
					__FUNCTION__);
		errno = EINVAL;

		return -1;
	}

	// Get the inode number for the file.
	inodestr = sstack_memcache_read_one(mc, path, strlen(path), &size, sfs_ctx);
	if (NULL == inodestr) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to retrieve the reverse lookup "
					"for path %s.\n", __FUNCTION__, path);
		errno = ENOENT;

		return -1;
	}
	inode_num = atoll((const char *)inodestr);
	ret = sfs_rdlock(inode_num);
	if (ret == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to obtain read lock for file "
						"%s\n", __FUNCTION__< path);
		errno = EAGAIN;

		return -1;
	}
	// Get inode from DB
	ret = get_inode(inode_num, &inode, db);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to get inode %lld. Path = %s "
						"error = %d\n", __FUNCTION__, inode_num, path, ret);
		errno = ret;
		sfs_unlock(inode_num);

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
				// Free up dynamically allocated fields in inode structure
				sstack_free_inode_res(&inode, sfs_ctx);
				sfs_unlock(inode_num);

				return -1;
			} else {
				ret = append_xattr(&inode, name, value, len);
				if (ret == -1) {
					sfs_log(sfs_ctx, SFS_ERR, "%s: Attribute create failed for"
								" file %s inode %lld . Error = %d \n",
								__FUNCTION__, path, inode_num, errno);
					// Free up dynamically allocated fields in inode structure
					sstack_free_inode_res(&inode, sfs_ctx);
					sfs_unlock(inode_num);
					return -1;
				} else {
					sstack_free_inode_res(&inode, sfs_ctx);
					sfs_unlock(inode_num);
					return 0;
				}
			}
		case XATTR_REPLACE:
			// Check if attribute exists
			// If not, return error
			if (xattr_exists(&inode, name) != 1)  {
				sfs_log(sfs_ctx, SFS_ERR, "%s: Attribute %s does not exist.",
								" Path = %s inode %lld attr %s\n",
								__FUNCTION__, name, path, inode_num, name);
				errno = ENOATTR;

				sstack_free_inode_res(&inode, sfs_ctx);
				sfs_unlock(inode_num);
				return -1;
			} else {
				ret = replace_xattr(&inode, name, value, len);
				if (ret == -1) {
					sfs_log(sfs_ctx, SFS_ERR, "%s: Attribute replace failed for"
								" file %s inode %lld . Error = %d \n",
								__FUNCTION__, path, inode_num, errno);
					sstack_free_inode_res(&inode, sfs_ctx);
					sfs_unlock(inode_num);
					return -1;
				} else {
					sstack_free_inode_res(&inode, sfs_ctx);
					sfs_unlock(inode_num);
					return 0;
				}
			}
		case 0:
			if (xattr_exists(&inode, name) == 1) {
				ret = replace_xattr(&inode, name, value, len);
				if (ret == -1) {
					sfs_log(sfs_ctx, SFS_ERR, "%s: Attribute replace failed for"
								" file %s inode %lld . Error = %d \n",
								__FUNCTION__, path, inode_num, errno);
					sstack_free_inode_res(&inode, sfs_ctx);
					sfs_unlock(inode_num);
					return -1;
				} else {
					sstack_free_inode_res(&inode, sfs_ctx);
					sfs_unlock(inode_num);
					return 0;
				}
			} else {
				ret = append_xattr(&inode, name, value, len);
				if (ret == -1) {
					sfs_log(sfs_ctx, SFS_ERR, "%s: Attribute create failed for"
								" file %s inode %lld . Error = %d \n",
								__FUNCTION__, path, inode_num, errno);
					sstack_free_inode_res(&inode, sfs_ctx);
					sfs_unlock(inode_num);
					return -1;
				} else {
					sstack_free_inode_res(&inode, sfs_ctx);
					sfs_unlock(inode_num);
					return 0;
				}
			}
		default:
			sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid flags specified \n");
			errno = EINVAL;
			sstack_free_inode_res(&inode, sfs_ctx);
			sfs_unlock(inode_num);

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

	// Hack Alert
	// TODO
	if (strcmp("security.ima", name) == 0)
		return -1;

	sfs_log(sfs_ctx, SFS_DEBUG, "%p %p %p %d\n", path, name, value, size);
	if (path)
		sfs_log(sfs_ctx, SFS_DEBUG, "path = %s\n", path);
	if (name)
		sfs_log(sfs_ctx, SFS_DEBUG, "name = %s\n", name);
	if (value)
		sfs_log(sfs_ctx, SFS_DEBUG, "value = %s\n", value);

	sfs_log(sfs_ctx, SFS_DEBUG, "%s() <<<<<\n", __FUNCTION__);
	// Parameter validation
	if (NULL == path || NULL == name  || NULL == value || size == 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters passed. \n",
					__FUNCTION__);
		errno = EINVAL;

		return -1;
	}

	if (strcmp(path, ".") == 0 || strcmp(path, "..") == 0) {
		name = NULL;
		value = NULL;
		return 0;
	}

	// Get the inode number for the file.
	inodestr = sstack_memcache_read_one(mc, path, strlen(path), &size1,
			sfs_ctx);
	if (NULL == inodestr) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to retrieve the reverse lookup "
					"for path %s.\n", __FUNCTION__, path);
		errno = ENOENT;

		return -1;
	}
	inode_num = atoll((const char *)inodestr);
	ret = sfs_rdlock(inode_num);
	if (ret == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to obtain read lock for file "
						"%s \n", __FUNCTION__, path);
		errno = EAGAIN;

		return -1;
	}
	// Get inode from DB
	ret = get_inode(inode_num, &inode, db);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to get inode %lld. Path = %s "
						"error = %d\n", __FUNCTION__, inode_num, path, ret);
		errno = ret;
		sfs_unlock(inode_num);

		return -1;
	}

	p = inode.i_xattr;
	sfs_log(sfs_ctx, SFS_DEBUG, "%s() - i_xattrlen: %d\n",
			__FUNCTION__, inode.i_xattrlen);
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
			sfs_unlock(inode_num);

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
	errno = EINVAL;
	sstack_free_inode_res(&inode, sfs_ctx);
	sfs_unlock(inode_num);

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

	sfs_log(sfs_ctx, SFS_DEBUG, "%s() <<<<<\n", __FUNCTION__);
	// Parameter validation
	if (NULL == path || NULL == list || size == 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);
		errno = EINVAL;

		return -1;
	}

	// Get the inode number for the file.
	inodestr = sstack_memcache_read_one(mc, path, strlen(path), &size1,
			sfs_ctx);
	if (NULL == inodestr) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to retrieve the reverse lookup "
					"for path %s.\n", __FUNCTION__, path);
		errno = ENOENT;

		return -1;
	}
	inode_num = atoll((const char *)inodestr);
	ret = sfs_rdlock(inode_num);
	if (ret == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to obtain read lock for file "
						"%s\n", __FUNCTION__, path);
		errno = EAGAIN;

		return -1;
	}
	// Get inode from DB
	ret = get_inode(inode_num, &inode, db);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to get inode %lld. Path = %s "
						"error = %d\n", __FUNCTION__, inode_num, path, ret);
		errno = ret;
		sfs_unlock(inode_num);

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

	sstack_free_inode_res(&inode, sfs_ctx);
	sfs_unlock(inode_num);

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

	sfs_log(sfs_ctx, SFS_DEBUG, "%s() <<<<< name = %s\n", __FUNCTION__, name);
	// Parameter validation
	if (NULL == inode || NULL == name) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);
		errno = EINVAL;
		return -1;
	}

	if (strcmp(name, "security.ima") != 0)  {
		if (NULL == inode->i_xattr || inode->i_xattrlen <= 0) {
			sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified \n",
							__FUNCTION__);
			errno = EINVAL;
			return -1;
		}
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

	sfs_log(sfs_ctx, SFS_DEBUG, "%s() <<<<<\n", __FUNCTION__);
	// Hack Alert
	// TODO
	if (strcmp("security.ima", name) == 0)
		return -1;

	// Parameter validation
	if (NULL == path || NULL == name) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters passed. \n",
					__FUNCTION__);
		errno = EINVAL;

		return -1;
	}


	// Get the inode number for the file.
	inodestr = sstack_memcache_read_one(mc, path, strlen(path), &size, sfs_ctx);
	if (NULL == inodestr) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to retrieve the reverse lookup "
					"for path %s.\n", __FUNCTION__, path);
		errno = ENOENT;

		return -1;
	}
	inode_num = atoll((const char *)inodestr);
	ret = sfs_wrlock(inode_num);
	if (ret == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to obtain write lock for file "
						"%s\n", __FUNCTION__, path);
		errno = EAGAIN;

		return -1;
	}
	// Get inode from DB
	ret = get_inode(inode_num, &inode, db);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to get inode %lld. Path = %s "
						"error = %d\n", __FUNCTION__, inode_num, path, ret);
		errno = ret;
		sfs_unlock(inode_num);

		return -1;
	}


	ret = xattr_remove(&inode, name);
	if (ret == -1 && (strcmp(name, "security.ima") != 0)) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Removing xattr %s failed for file %s\n",
						__FUNCTION__, name, path);
		errno = ENOATTR;
		sstack_free_inode_res(&inode, sfs_ctx);
		sfs_unlock(inode_num);
		return -1;
	} else if (strcmp(name, "security.ima") == 0) {
		return 0;
	}

	// Update inode
	ret = put_inode(&inode, db, 1);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: put inode failed. Error = %d\n",
						__FUNCTION__, errno);
		// Don't modify errno set by put_inode
		sstack_free_inode_res(&inode, sfs_ctx);
		sfs_unlock(inode_num);
		return -1;
	}

	sstack_free_inode_res(&inode, sfs_ctx);
	sfs_unlock(inode_num);

	return 0;
}

void
sfs_destroy(void *arg)
{

	sfs_log(sfs_ctx, SFS_DEBUG, "%s() <<<<<\n", __FUNCTION__);
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

	sfs_log(sfs_ctx, SFS_DEBUG, "%s() <<<<<\n", __FUNCTION__);
	// Parameter validation
	if (NULL == path || mode == 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);
		errno = EINVAL;

		return -1;
	}

	if (strcmp(path, "/") == 0)
		return 0;

	// Get the inode number for the file
	inodestr = sstack_memcache_read_one(mc, path, strlen(path), &size, sfs_ctx);
	if (NULL == inodestr) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to retrieve the reverse lookup "
						"for path %s.\n", __FUNCTION__, path);
		errno = ENOENT;

		return -1;
	}
	inode_num = atoll((const char *) inodestr);
	ret = sfs_rdlock(inode_num);
	if (ret == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to obtain read lcok for file "
						"%s\n", __FUNCTION__, path);
		errno = EAGAIN;

		return -1;
	}
	// Get inode from db
	ret = get_inode(inode_num, &inode, db);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to get inode %lld. Path = %s "
						"error = %d\n", __FUNCTION__, inode_num, path, ret);
		errno = ret;
		sfs_unlock(inode_num);

		return -1;
	}

	if (mode & F_OK) {
		// Application is checking whether file exists
		// Since file info is in db, file exists.
		sstack_free_inode_res(&inode, sfs_ctx);
		sfs_unlock(inode_num);

		return 0;
	}

	if (inode.i_mode & mode) {
		// Mode is a subset of what is set in inode
		sstack_free_inode_res(&inode, sfs_ctx);
		sfs_unlock(inode_num);

		return 0;
	 } else {
		sstack_free_inode_res(&inode, sfs_ctx);
		errno = EACCES;
		sfs_unlock(inode_num);

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
	sstack_inode_t *inode = NULL;
	int ret = -1;
	int fd = -1;
	char inode_str[MAX_INODE_LEN] = { '\0' };
	char *fullpath = NULL;
	struct timespec ts;
	
	sfs_log(sfs_ctx, SFS_DEBUG, "%s() - enter\n", __FUNCTION__);
	if (NULL == path) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified \n",
				__FUNCTION__);
		errno = EINVAL;
		return -1;
	}

	fullpath = prepend_mntpath(path);
    sfs_log(sfs_ctx, SFS_DEBUG, "%s: path = %s\n", __FUNCTION__, fullpath);
	sfs_log(sfs_ctx, SFS_DEBUG, "%s() - file handle : %d\n",
			__FUNCTION__, fi->fh);
	fd = creat(fullpath, mode);
	if (fd == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Create failed with error %d \n",
				__FUNCTION__, errno);
		errno = EACCES;
		free(fullpath);
		return -1;
	}
	fi->fh = fd;
	free(fullpath);

	inode = sstack_create_inode();

	if (inode == NULL) {
		sfs_log(sfs_ctx, SFS_ERR, "Inode create failed\n");
		unlink(fullpath);
		return -ENOMEM;
	}

	// Populate DB with new inode info
	inode->i_num = get_free_inode();
	strcpy(inode->i_name, path);

	inode->i_uid = getuid();
	inode->i_gid = getgid();
	inode->i_mode = mode;
	switch (mode & S_IFMT) {
		case S_IFDIR:
			inode->i_type = DIRECTORY;
			break;
		case S_IFREG:
			inode->i_type = REGFILE;
			break;
		case S_IFLNK:
			inode->i_type = SYMLINK;
			break;
		case S_IFBLK:
			inode->i_type = BLOCKDEV;
			break;
		case S_IFCHR:
			inode->i_type = CHARDEV;
			break;
		case S_IFIFO:
			inode->i_type = FIFO;
			break;
		case S_IFSOCK:
			inode->i_type = SOCKET_TYPE;
			break;
		default:
			inode->i_type = UNKNOWN;
			break;
	}

	// Make sure we are not looping
	// TBD

	clock_gettime(CLOCK_MONOTONIC, &ts);
	memcpy(&inode->i_atime, &ts, sizeof(struct timespec));
	memcpy(&inode->i_ctime, &ts, sizeof(struct timespec));
	memcpy(&inode->i_mtime, &ts, sizeof(struct timespec));
	inode->i_links = 1;
	ret = put_inode(inode, db, 0);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to store inode #%lld for "
				"path %s. Error %d\n", __FUNCTION__, inode->i_num,
				inode->i_name, errno);
		close(fd);
		return -1;
	}

	// Populate memcached for reverse lookup
	sprintf(inode_str, "%lld", inode->i_num);
	ret = sstack_memcache_store(mc, path, inode_str, (strlen(inode_str) + 1),
					sfs_ctx);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to store object into memcached."
				" Key = %s value = %s \n", __FUNCTION__, path, inode_str);
		close(fd);
		return -1;
	}
	sstack_dump_inode(inode, sfs_ctx);
	memset(inode, 0, sizeof(*inode));
	get_inode(3, inode, db);
	sstack_dump_inode(inode, sfs_ctx);
	sstack_free_inode(inode);
	sfs_log(sfs_ctx, SFS_DEBUG, "%s() - file created, return: %d\n",
			__FUNCTION__, fd);
	return 0;
}

/*
 * sfs_ftruncate - Truncate the file to 'offset' bytes
 *
 * path - Path name of the file
 * offset - Length of the file after the operation if successful
 * fi - FUSE handle of the file
 *
 * Returns 0 on success and -1 on failure.
 */
int
sfs_ftruncate(const char *path, off_t offset, struct fuse_file_info *fi)
{
	sstack_inode_t inode;
	int ret = -1;
	char *inodestr = NULL;
	unsigned long long inode_num = 0;
	size_t size = 0;

	sfs_log(sfs_ctx, SFS_DEBUG, "%s() <<<<<\n", __FUNCTION__);
	// Parameter validation
	if (NULL == path || NULL == fi) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);
		errno = ENOENT;
		return -1;
	}

	ret = ftruncate(fi->fh, offset);
	if (ret == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: ftruncate failed for file %s with "
						"error %d \n", __FUNCTION__, path, errno);
		return -1;
	}

	// Get the inode number for the file.
	inodestr = sstack_memcache_read_one(mc, path, strlen(path), &size, sfs_ctx);
	if (NULL == inodestr) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to retrieve the reverse lookup "
					"for path %s.\n", __FUNCTION__, path);
		errno = ENOENT;

		return -1;
	}
	inode_num = atoll((const char *)inodestr);
	ret = sfs_wrlock(inode_num);
	if (ret == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to obtain write lock for file "
						"%s\n", __FUNCTION__, path);
		errno = EAGAIN;

		return -1;
	}
	// Get inode from DB
	ret = get_inode(inode_num, &inode, db);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to get inode %lld. Path = %s "
						"error = %d\n", __FUNCTION__, inode_num, path, ret);
		errno = ret;
		sfs_unlock(inode_num);

		return -1;
	}

	// Update inode size
	inode.i_size = offset;
	// TODO
	// Remove the extent information and sfsds to remove the extent files
	// There is no NFSv3 operation for truncate. We need to device a new
	// operation

	// Store inode
	ret = put_inode(&inode, db, 1);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to store inode #%lld for "
				"path %s. Error %d\n", __FUNCTION__, inode.i_num,
				inode.i_name, errno);
		sstack_free_inode_res(&inode, sfs_ctx);
		sfs_unlock(inode_num);
		return -1;
	}

	// Free up dynamically allocated fields in inode structure
	sstack_free_inode_res(&inode, sfs_ctx);
	sfs_unlock(inode_num);

	return 0;
}

/*
 * sfs_utimens - utimens entry point for sfs
 *
 * path - file name
 * tv - Time to be updated
 *
 * Returns 0 on success and -1 on failure.
 */

int
sfs_utimens(const char *path, const struct timespec tv[2])
{
	int ret = -1;
	char *fullpath = NULL;

	sfs_log(sfs_ctx, SFS_DEBUG, "%s() <<<<<\n", __FUNCTION__);
	// Parameter validation
	if (NULL == path) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameter specified \n",
						__FUNCTION__);
		errno = EINVAL;

		return -1;
	}

	fullpath = prepend_mntpath(path);

	// Call utimensat which is the closest to the intended functionality
	ret = utimensat(0, fullpath, tv, AT_SYMLINK_NOFOLLOW);
	free(fullpath);

	return ret;
}

/* This is a low priority independent job. So no association with the
   calling thread needed for this job
 */
static inline void
sfs_submit_esure_code_job(sstack_inode_t inode, off_t offset, size_t size)
{
	sfs_job_t *job = NULL;
	sstack_payload_t *payload = NULL;
	int		ret = 0;

	sfs_log(sfs_ctx, SFS_DEBUG, "%s() <<<<<\n", __FUNCTION__);
	job = sfs_job_init();
    if (NULL == job) {
	    sfs_log(sfs_ctx, SFS_ERR, "%s: Fail to allocate job memory.\n",
                                  __FUNCTION__);
        return;
    }

	job->id = get_next_job_id();
	job->job_type = SFSD_IO;
	job->num_clients = 1;
	job->sfsds[0] = inode.i_primary_sfsd->sfsd;
	job->job_status[0] = JOB_STARTED;
	job->priority = QOS_LOW;
    payload = sstack_create_payload(SSTACK_SFS_ASYNC_CMD);
    payload->hdr.sequence = 0; // Reinitialized by transport
    payload->hdr.payload_len = sizeof(sstack_payload_t);
    payload->hdr.job_id = job->id;
    payload->hdr.priority = job->priority;
	payload->hdr.arg = (uint64_t) job;
	payload->command = NFS_ESURE_CODE;
	payload->command_struct.esure_code_cmd.inode_no = inode.i_num;
	payload->command_struct.esure_code_cmd.num_blocks = 1;
	payload->command_struct.esure_code_cmd.ext_blocks =
							malloc(sizeof (sstack_extent_range_t));
	if (payload->command_struct.esure_code_cmd.ext_blocks == NULL) {
		free(job);
		free(payload);
		return;
	}
	// populate extent
	job->payload_len = sizeof(sstack_payload_t);
    job->payload = payload;

	ret = sfs_submit_job(job->priority, jobs, job);
    if (ret == -1) {
		// sfs_log here
	}
}

/* Send response function for various commands */
static inline int
sfs_send_read_status(sstack_job_map_t *job_map, char *buf, size_t size)
{
	int                     i = 0;
    uint32_t                num_bytes = 0;
    sstack_job_id_t         job_id;
    sstack_jt_t             *jt_node = NULL, jt_key;
    sfs_job_t               *job = NULL;
    struct sstack_nfs_read_resp    read_resp;

	sfs_log(sfs_ctx, SFS_DEBUG, "%s() <<<<<\n", __FUNCTION__);
	/* Not all jobs processed and we got a pthread_cond_signal.
       Some job had a read specific error */
    if (job_map->num_jobs_left != 0) {
		errno = job_map->err_no;
        return (-1);
    }

	/* Success case. Return the number of bytes read */
	for (i = 0; i < job_map->num_jobs; i++) {
	    job_id = job_map->job_ids[i];

		jt_key.magic = JTNODE_MAGIC;
        jt_key.job_id = job_id;
        jt_node = jobid_tree_search(jobid_tree, &jt_key);
        job = jt_node->job;

		read_resp = job->payload->response_struct.read_resp;
		memcpy(buf, read_resp.data.data_buf, sizeof(uint8_t));
		buf += read_resp.data.data_len;
		num_bytes += read_resp.data.data_len;

		sfs_job2thread_map_remove(job_id);
		free(job->payload);
		free(job);
	}

	return (num_bytes);
}

static inline int
sfs_send_write_status(sstack_job_map_t *job_map,  sstack_inode_t inode,
						off_t offset, size_t size)
{
	int                     i = 0;
    uint32_t                num_bytes = 0;
    sstack_job_id_t         job_id;
    sstack_jt_t             *jt_node = NULL, jt_key;
    sfs_job_t               *job = NULL;

	sfs_log(sfs_ctx, SFS_DEBUG, "%s() <<<<<\n", __FUNCTION__);
	for (i = 0; i < job_map->num_jobs; i++) {
	    job_id = job_map->job_ids[i];

		jt_key.magic = JTNODE_MAGIC;
        jt_key.job_id = job_id;
        jt_node = jobid_tree_search(jobid_tree, &jt_key);
        job = jt_node->job;

		sfs_log(sfs_ctx, SFS_DEBUG, "%s() error: %d\n", __FUNCTION__,
				job_map->err_no);
		sfs_log(sfs_ctx, SFS_DEBUG, "%s() status: %d\n", __FUNCTION__,
				job->payload->response_struct.command_ok);
		//if (job_map->err_no == SSTACK_SUCCESS) {
		if (job->payload->response_struct.command_ok == SSTACK_SUCCESS) {
			num_bytes += job->payload->response_struct.write_resp.file_wc;
			sfs_log(sfs_ctx, SFS_DEBUG, "%s() num_bytes: %d\n", __FUNCTION__,
				num_bytes);
			sfs_log(sfs_ctx, SFS_DEBUG, "%s() num_bytes: %d\n", __FUNCTION__,
				num_bytes);
		} else {
		/* Make the i_extent NULL. Once job_map->err_no shows an
		 * error, we should invalidate all the i_extents that are part
		 * of this job_map to follow all or none(either all extents
		 * are properly written or none are written) approach.
		 */
		}
		sfs_job2thread_map_remove(job_id);
		sstack_free_payload(job->payload);
		free(job);
	}
	sfs_log(sfs_ctx, SFS_DEBUG, "%s() - Returning %d bytes\n",
			__FUNCTION__, num_bytes);
	return num_bytes;
	//Short circuiting for debugging
#if 0
	if (job_map->err_no == SSTACK_SUCCESS) {
		if (job_map->op_status[0] == JOB_FAILED) {
			/* Primary sfsd job failed. Some recovery needed
			 * before issuing erasure code job
			 */
		} else {
			sfs_submit_esure_code_job(inode, offset, size);
		}
		return (num_bytes);
	} else {
		errno = job_map->err_no;
		return (-1);
	}
#endif
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
	sfs_log(sfs_ctx, SFS_DEBUG, "%s() <<<<<\n", __FUNCTION__);
	errno = ENOSYS;
	return -1;
}

int
sfs_ioctl(const char *path, int cmd, void *arg, struct fuse_file_info *fi,
				unsigned int flags, void *data)
{
	sfs_log(sfs_ctx, SFS_DEBUG, "%s() <<<<<\n", __FUNCTION__);
	errno = ENOSYS;
	return -1;
}

int
sfs_poll(const char *path, struct fuse_file_info *fi,
				struct fuse_pollhandle *ph, unsigned *reventsp)
{
	sfs_log(sfs_ctx, SFS_DEBUG, "%s() <<<<<\n", __FUNCTION__);
	errno = ENOSYS;
	return -1;
}

int
sfs_write_buf(const char *path, struct fuse_bufvec *buf, off_t off,
				struct fuse_file_info *fi)
{
	sfs_log(sfs_ctx, SFS_DEBUG, "%s() <<<<<\n", __FUNCTION__);
	errno = ENOSYS;
	return -1;
}

int
sfs_read_buf(const char *path, struct fuse_bufvec **bufp, size_t size,
				off_t off, struct fuse_file_info *fi)
{
	sfs_log(sfs_ctx, SFS_DEBUG, "%s() <<<<<\n", __FUNCTION__);
	errno = ENOSYS;
	return -1;

}

int
sfs_mknod(const char *path, mode_t mode, dev_t rdev)
{
	sfs_log(sfs_ctx, SFS_DEBUG, "%s() <<<<<\n", __FUNCTION__);
	errno = ENOSYS;
	return -1;
}
