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

int
sfs_getattr(const char *path, struct stat *stbuf)
{

	return 0;	
}


int
sfs_readlink(const char *path, char *buf, size_t size)
{

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

int
sfs_setxattr(const char *path, const char *name, const char *value,
	size_t size, int flags)
{

	return 0;	
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
sfs_read_buf(const char *path, struct fuse_bufvec *bufp, size_t size,
				off_t off, struct fuse_file_info *fi)
{

	return 0;	
}

int
sfs_flock(const char *path, struct fuse_file_info *fi, int op)
{

	return 0;	
}
