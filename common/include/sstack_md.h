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

#ifndef __SSTACK_MD_H_
#define __SSTACK_MD_H_

#ifdef USE_FUTEX
#include <linux/futex.h>
#include <sys/time.h>
#else
#include <pthread.h>
#endif /* USE_FUTEX */
#include <linux/limits.h>
#include <time.h>
#include <syslog.h>

#include <sstack_policy.h>
#include <sstack_bitops.h>
#include <sstack_db.h>


#define MAX_REPLICAS 3  // Recommended number of replicas in standard practice
#define INODE_NUM_START 2

typedef enum type {
	REGFILE = 1,
	DIRECTORY,
	SYMLINK,
	HARDLINK,
	BLOCKDEV,
	CHARDEV,
	SOCKET_TYPE,
	FIFO,
	UNKNOWN
} type_t;

// TBD
// Other attribute types (if required) need to be added and extent_t
// needs to be updated
typedef enum attr_type {
	OFFSET = 1
} attr_type_t;
	

// Defines chunk location . Overloading on xattr
typedef struct extent {
	attr_type_t e_type;
	uint64_t e_value; // Like offset in file system blocks. Max value is 2^76 or 65536 exa bytes
	uint64_t e_size; // in bytes. So max chunk size is 16 exa bytes
	unsigned long e_cksum; // Checksum of the chunk
	uint8_t e_replica_valid; // Validity of replicas. For self healing or migrating chunks to other storage	
	char e_path[MAX_REPLICAS][PATH_MAX]; // File name on the distributed FS
} extent_t;

// Defines metadata structure for each inode
// If i_type is SYMLINK, first extent file name contains the real file name 
// to which this inode/file links.

typedef struct inode {
#ifndef USE_FUTEX
	pthread_mutex_t	i_lock; // We could use futex 
#endif /* USE_FUTEX */
	uint64_t i_num; // Inode number . max number of files possible is 2^64
	char i_name[PATH_MAX]; 
	uid_t	i_uid; // Owner uid
	gid_t	i_gid; // Owner gid
	mode_t	i_mode; // Permissions
	type_t	i_type; // Type of file	
	int		i_links; // Number of references. Used for unlink()
	policy_t i_policy; 
	struct timespec i_atime; // Last access time
	struct timespec i_ctime; // Creation time
	struct timespec i_mtime; // Modification time
	uint64_t	i_size; // Size of the file
	int i_numreplicas; // Number of replicas
	int i_numextents; // Number of extents
	char i_erasure_code_path[PATH_MAX]; // Erasure code for the file
	extent_t i_extent[0]; // extents 
} inode_t;


extern int get_inode(uint64_t inode_num, inode_t * inode, db_t *db);
extern int get_extents(uint64_t inode_num, extent_t *extent, int num_extents,
					db_t *db);
extern uint64_t get_inode_number(const char *path);
unsigned long checksum(const char *filename);
void dump_extents(extent_t *extent, int num_extents);

// Helper functions
static inline int
get_inode_size(uint64_t inode_num, db_t *db)
{
	inode_t inode;
	int ret = -1;
	// Go read the initial inode structure without extents

	ret = get_inode(inode_num, &inode, db);
	if (ret != -1)
		return (sizeof(inode_t) + (inode.i_numextents * sizeof(extent_t)));
	else 
		return -1;
}

extern uint64_t inode_number;
extern struct hashtable *reverse_lookup;
extern pthread_mutex_t inode_mutex;


// This is for releasing an inode numer to inode cache
// Inode cache contains recently released inode numbers for reuse
static inline void
release_inode(uint64_t inode_num)
{
	return;
}

// This is for allocating new inode number

static inline uint64_t
get_free_inode(void)
{
	// Check inode cache for any reusable inodes
	// If there are reusable inodes, remove the first one from inode cache
	// and return the inode_num
	// If there are none, atomic increment the global inode number and 
	// return preincremented global inode number
	// Global inode number needs to be preserved in superblock which will 
	// be stored as an object in database.
	pthread_mutex_lock(&inode_mutex);
	inode_number ++;
	pthread_mutex_unlock(&inode_mutex);

//	USYSLOG(LOG_ERR, "%s: Returning inode number %d \n", __FUNCTION__, (int)inode_number);

	return inode_number;
}

#endif /* __SSTACK_MD_H_ */
