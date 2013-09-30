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
#include <sfs.h>
#include <sfs_entry.h>
#include <sstack_helper.h>
#include <sstack_cache_api.h>

#define MAX_KEY_LEN 128

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
	inodestr = sstack_cache_read_one(mc, path, strlen(path), &size);
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
	inodestr = sstack_cache_read_one(mc, path, strlen(path), &size);
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


int
sfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
	off_t offset, struct fuse_file_info *fi)
{

	return 0;	
}

int
sfs_mknod(const char *path, mode_t mode, dev_t rdev)
{

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

	return 0;	
}

int
sfs_write(const char *path, const char *buf, size_t size, off_t offset,
	struct fuse_file_info *fi)
{

	return 0;	
}

int
sfs_statfs(const char *path, struct statvfs *stbuf)
{

	return 0;	
}

int
sfs_flush(const char *path, struct fuse_file_info *fi)
{

	return 0;	
}

int
sfs_release(const char *path, struct fuse_file_info *fi)
{

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
	memcpy((void *) (p + cur_len) , (void *) name, key_len);
	cur_len  += key_len;
	// Append '='
	memcpy((void *) (p + cur_len) , (void *)  "=", 1);
	cur_len += 1;
	// Append value
	memcpy((void *) (p + cur_len) ,(void *) value, len);
	cur_len += len;
	// Append '\0'
	// "0" is same as '\0'
	memcpy((void *) (p + cur_len) , (void *) "0", 1);
	cur_len += 1;

	inode->i_xattrlen = cur_len;
	inode->i_xattr = p;

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
	inodestr = sstack_cache_read_one(mc, path, strlen(path), &size);
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

int
sfs_getxattr(const char *path, const char *name, char *value, size_t size)
{

	return 0;	
}

int
sfs_listxattr(const char *path, char *list, size_t size)
{

	return 0;	
}

int
sfs_removexattr(const char *path, const char *name)
{

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

int
sfs_access(const char *path, int mode)
{

	return 0;	
}

int
sfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{

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

/*
 *  Provide block map (as in FIBMAP) for the given offset
 *  Can not be implemented.
 */
int
sfs_bmap(const char *path, size_t blocksize, uint64_t *idx)
{

	return 0;	
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

	return 0;	
}

int
sfs_read_buf(const char *path, struct fuse_bufvec **bufp, size_t size,
				off_t off, struct fuse_file_info *fi)
{

	return 0;	
}

int
sfs_flock(const char *path, struct fuse_file_info *fi, int op)
{

	return 0;	
}
