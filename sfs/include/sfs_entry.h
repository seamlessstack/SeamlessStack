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

#ifndef __SFS_ENTRY_H_
#define __SFS_ENTRY_H_

#include <stdint.h>
#include <sstack_cache_api.h>
#include <sfs_jobmap_tree.h>
#include <sfs_jobid_tree.h>
#include <sfs_lock_tree.h>

extern unsigned long long max_inode_number;
extern memcached_st *mc;
extern jobmap_tree_t *jobmap_tree;
extern jobid_tree_t *jobid_tree;

// SFS FUSE entry points
extern int sfs_getattr(const char *, struct stat *);
extern int sfs_readlink(const char *, char *, size_t );
extern int sfs_readdir(const char *, void *, fuse_fill_dir_t ,
	off_t , struct fuse_file_info *);
extern int sfs_mknod(const char *, mode_t , dev_t );
extern int sfs_mkdir(const char *, mode_t );
extern int sfs_unlink(const char * );
extern int sfs_rmdir(const char * );
extern int sfs_symlink(const char *, const char * );
extern int sfs_rename(const char *, const char * );
extern int sfs_link(const char *, const char * );
extern int sfs_chmod(const char *, mode_t );
extern int sfs_chown(const char *, uid_t , gid_t );
extern int sfs_truncate(const char *, off_t );
extern int sfs_open(const char *, struct fuse_file_info *);
extern int sfs_read(const char *, char *, size_t , off_t ,
	struct fuse_file_info *);
extern int sfs_write(const char *, const char *, size_t , off_t ,
	struct fuse_file_info *);
extern int sfs_statfs(const char *, struct statvfs *);
extern int sfs_flush(const char *, struct fuse_file_info *);
extern int sfs_release(const char *, struct fuse_file_info *);
extern int sfs_fsync(const char *, int , struct fuse_file_info *);
extern int sfs_setxattr(const char *, const char *, const char *,
	size_t , int );
extern int sfs_getxattr(const char *, const char *, char *, size_t );
extern int sfs_listxattr(const char *, char *, size_t );
extern int sfs_removexattr(const char *, const char *);
extern int sfs_opendir(const char *, struct fuse_file_info *);
extern int sfs_releasedir(const char *, struct fuse_file_info *);
extern int sfs_fsyncdir(const char *, int , struct fuse_file_info *);
extern void sfs_destroy(void *);
extern int sfs_access(const char *, int );
extern int sfs_create(const char *, mode_t , struct fuse_file_info *);
extern int sfs_ftruncate(const char *, off_t , struct fuse_file_info *);
extern int sfs_fgetattr(const char *, struct stat *, struct fuse_file_info *);
extern int sfs_lock(const char *, struct fuse_file_info *, int ,
			struct flock *);
extern int sfs_utimens(const char *, const struct timespec tv[2]);
extern int sfs_bmap(const char *, size_t , uint64_t *);
extern int sfs_ioctl(const char *, int , void *, struct fuse_file_info *,
				unsigned int , void *);
extern int sfs_poll(const char *, struct fuse_file_info *,
				struct fuse_pollhandle *, unsigned *);
extern int sfs_write_buf(const char *, struct fuse_bufvec *, off_t ,
				struct fuse_file_info *);
extern int sfs_read_buf(const char *, struct fuse_bufvec **, size_t ,
				off_t , struct fuse_file_info *);
extern int sfs_flock(const char *, struct fuse_file_info *, int );

#endif // __SFS_ENTRY_H_
