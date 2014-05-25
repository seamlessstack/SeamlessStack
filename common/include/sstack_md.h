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
#include <sys/param.h>
#include <stdint.h>
#include <policy.h>
#include <sstack_bitops.h>
#include <sstack_db.h>
#include <sstack_nfs.h>
#include <sstack_transport.h>

// Recommended number of replicas in standard practice is 3.
// Setting MAX_REPLICAS to 10 just to handle user request
#define MAX_REPLICAS 10
#define INODE_NUM_START 2
#define SSTACK_EXTENT_SIZE 65536

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

typedef unsigned long long sstack_offset_t;
typedef unsigned long long sstack_size_t;

// Defines chunk location . Overloading on xattr
typedef struct extent {
	// Fixed fields of the extent
	// Used in get_extent_fixed_fields_len()
	struct {
		sstack_offset_t e_offset; // Offset within the file
		uint64_t e_size; // Though current extent size is fixed at 64KiB, we
		// still need this field to figure out partially filled up extent.
		uint64_t e_sizeondisk; // On disk size after storage policy apply
		sstack_cksum_t e_cksum; // Checksum of the extent
		unsigned int e_numreplicas; // Avoid reading sstack_inode_t again
	};
	sstack_file_handle_t *e_path; // Pointer to an array of replica paths
} sstack_extent_t;


/*
 * get_extent_fixed_fields_len - Return length of fixed fields in extent
 *
 * Returns size of fixed length fields in sstack_extent_t
 *
 * Note:
 * PLEASE MODIFY THIS FUNCTION WHEN FIXED FIELDS OF SSTACK_EXTENT_T CHANGE.
 */

static inline int
get_extent_fixed_fields_len(void)
{
	return (sizeof(sstack_offset_t) + sizeof(uint64_t) + sizeof(uint64_t) +
			sizeof(sstack_cksum_t) + sizeof(unsigned int));
}



// Defines erasure code location
// For now, path is sufficient. This structure is for future enhancements.
typedef struct erasure {
	sstack_file_handle_t *er_path;
	sstack_cksum_t er_cksum; // Checksum for erasure codes
} sstack_erasure_t;

// Defines sfsds maintaining a file
// transport uniquely identifies the sfsd. This is the only valid field
// that is stored on DB. sfsd field is only valid when sfs is running.
// sfsd field needs to be reinitialized when sfs restarts

typedef struct sstack_sfsd_info {
	sstack_transport_t transport; // Field that uniquely represents sfsd
	sfsd_t *sfsd; // Transient
} sstack_sfsd_info_t;

// Defines metadata structure for each inode
// If i_type is SYMLINK, first extent file name contains the real file name
// to which this inode/file links.

typedef struct inode {
	// Following structure indicates fixed fields in sstack_inode_t
	// Remaining fileds are variable in nature.
	struct {
		pthread_mutex_t	i_lock; // We could use futex
		char i_name[PATH_MAX];
		uid_t	i_uid; // Owner uid
		gid_t	i_gid; // Owner gid
		mode_t	i_mode; // Permissions
		type_t	i_type; // Type of file
		struct timespec i_atime; // Last access time
		struct timespec i_ctime; // Creation time
		struct timespec i_mtime; // Modification time
		sstack_size_t	i_size; // Size of the file
		sstack_size_t i_ondisksize;
		sstack_sfsd_info_t *i_primary_sfsd; // sfsd having erasure coded stripes
		int32_t	i_links; // Number of references. Used for unlink()
		int32_t i_numreplicas; // Number of replicas
		int32_t i_enable_dr; // DR enable flag
		int32_t i_numclients; // Number of sfsds maintaining this file
		uint32_t i_numerasure; // Number of erasure code extents
		int32_t	i_esure_valid; // Is erasure code valid?
		int32_t i_numextents; // Number of extents
		int32_t i_xattrlen; // Extended attibute len
		// Inode number. Max is 2^128 on 64-bit machine
		uint64_t i_num;
		uint64_t i_erasure_stripe_size; // Erasure code stripe size
	};
	char *i_xattr; // Extended attributes
	sstack_extent_t *i_erasure; // Erasure code segment information
	sstack_extent_t *i_extent; // extents
	sstack_sfsd_info_t *i_sfsds; // sfsds maintaining this file
} sstack_inode_t;

/*
 * get_inode_fixed_fields_len - Return length of fixed fields of sstack_inode_t
 *
 * Retruns length of fixed fields.
 * PLEASE MODIFY THIS FUNCTION WHEN SET OF FIXED FIELDS CHANGE
 */

static inline int
get_inode_fixed_fields_len(void)
{
	return (sizeof(pthread_mutex_t) + PATH_MAX + sizeof(uid_t) + sizeof(gid_t)
			+ sizeof(mode_t) + sizeof(type_t) + (3 * sizeof(struct timespec))
			+ (2 *sizeof(sstack_size_t)) + sizeof(sstack_sfsd_info_t *) +
			+ 32 /* All uint/int32_t's */ + 16 /* All uint64_t's */);
}
extern int get_extents(unsigned long long  inode_num, sstack_extent_t *extent,
				int num_extents, db_t *db);
extern uint64_t get_inode_number(const char *path);
unsigned long checksum(const char *filename);
void dump_extents(sstack_extent_t *extent, int num_extents);

#endif /* __SSTACK_MD_H_ */
