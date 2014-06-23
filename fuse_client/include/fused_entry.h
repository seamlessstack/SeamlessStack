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

#ifndef __FUSED_ENTRY_H_
#define __FUSED_ENTRY_H_

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
extern int fused_getattr(const char *, struct stat *);
extern int fused_readlink(const char *, char *, size_t );
extern int fused_readdir(const char *, void *, fuse_fill_dir_t ,
	off_t , struct fuse_file_info *);
extern int fused_mknod(const char *, mode_t , dev_t );
extern int fused_mkdir(const char *, mode_t );
extern int fused_unlink(const char * );
extern int fused_rmdir(const char * );
extern int fused_symlink(const char *, const char * );
extern int fused_rename(const char *, const char * );
extern int fused_link(const char *, const char * );
extern int fused_chmod(const char *, mode_t );
extern int fused_chown(const char *, uid_t , gid_t );
extern int fused_truncate(const char *, off_t );
extern int fused_open(const char *, struct fuse_file_info *);
extern int fused_read(const char *, char *, size_t , off_t ,
	struct fuse_file_info *);
extern int fused_write(const char *, const char *, size_t , off_t ,
	struct fuse_file_info *);
extern int fused_statfs(const char *, struct statvfs *);
extern int fused_release(const char *, struct fuse_file_info *);
extern int fused_fsync(const char *, int , struct fuse_file_info *);
extern int fused_setxattr(const char *, const char *, const char *,
	size_t , int );
extern int fused_getxattr(const char *, const char *, char *, size_t );
extern int fused_listxattr(const char *, char *, size_t );
extern int fused_removexattr(const char *, const char *);
extern int fused_opendir(const char *, struct fuse_file_info *);
extern int fused_releasedir(const char *, struct fuse_file_info *);
extern int fused_fsyncdir(const char *, int , struct fuse_file_info *);
extern void fused_destroy(void *);
extern int fused_access(const char *, int );
extern int fused_create(const char *, mode_t , struct fuse_file_info *);
extern int fused_ftruncate(const char *, off_t , struct fuse_file_info *);
extern int fused_fgetattr(const char *, struct stat *, struct fuse_file_info *);
extern int fused_utimens(const char *, const struct timespec tv[2]);
extern int fused_bmap(const char *, size_t , uint64_t *);
extern int fused_ioctl(const char *, int , void *, struct fuse_file_info *,
				unsigned int , void *);
extern int fused_poll(const char *, struct fuse_file_info *,
				struct fuse_pollhandle *, unsigned *);
extern int fused_write_buf(const char *, struct fuse_bufvec *, off_t ,
				struct fuse_file_info *);
extern int fused_read_buf(const char *, struct fuse_bufvec **, size_t ,
				off_t , struct fuse_file_info *);

#endif // __FUSED_ENTRY_H_
