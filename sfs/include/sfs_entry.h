/*
 * Copyright (C) 2014 SeamlessStack
 *
 *  This file is part of SeamlessStack distributed file system.
 * 
 * SeamlessStack distributed file system is free software: you can redistribute
 * it and/or modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 * 
 * SeamlessStack distributed file system is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with SeamlessStack distributed file system. If not,
 * see <http://www.gnu.org/licenses/>.
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

#endif // __SFS_ENTRY_H_
