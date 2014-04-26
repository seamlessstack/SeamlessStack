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

#ifndef __SFS_INTERNAL_H__
#define __SFS_INTERNAL_H__

#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <sstack_jobs.h>
#include <bds_list.h>
#include <sstack_log.h>
#include <sstack_md.h>
#include <sstack_helper.h>

#define MAX_SFSD_POOLS 8 // pools of different storage weight range
#define MAXIMUM_WEIGHT 0
#define DEFAULT_WEIGHT 50
#define MINIMUM_WEIGHT 100
#define CACHE_NAME_MAX 32


// Represents storage weight range per pool
typedef struct storage_weight {
	uint32_t low;
	uint32_t high;
} sstack_storage_weight_t;

// TODO
// REVISIT BASED ON NEED
// Adjust MAX_SFSD_POOLS accordingly.
static sstack_storage_weight_t weights[MAX_SFSD_POOLS] [2] = { {{0}, {10}},
		{{11}, {20}}, {{21}, {30}}, {{31}, {40}}, {{41}, {50}}, {{51}, {60}},
		{{61}, {70}}, {{71}, {100}}};

// Structure representing sfsd pool for a given range
// list of sfsds for a given weight is for future extension. If a given
// chunk domain has many chunks, the we can create a new sfsd node and assign
// some of the chunks to new sfsd.
typedef struct sstack_sfsd_pool {
	sstack_storage_weight_t weight_range;
	bds_int_list_t list;
	pthread_spinlock_t lock;
} sstack_sfsd_pool_t;

extern log_ctx_t *sfs_ctx;
extern sstack_sfsd_pool_t *sfsd_pool;
extern bds_cache_desc_t sfs_global_cache[];
extern sfs_job_queue_t *jobs;
extern sfs_job_queue_t *pending_jobs;


inline sstack_sfsd_pool_t * sstack_sfsd_pool_init(void);
inline int sstack_sfsd_pool_destroy(sstack_sfsd_pool_t *);
inline int sstack_sfsd_add(uint32_t , sstack_sfsd_pool_t *, sfsd_t *);
inline int sstack_sfsd_remove(uint32_t , sstack_sfsd_pool_t *, sfsd_t *);

struct sfs_cache_entry {
	char name[CACHE_NAME_MAX];
	size_t size;
};

extern sstack_payload_t *  create_payload(sstack_command_t cmd);


extern unsigned long long  inode_number;
extern unsigned long long  active_inodes;
extern pthread_mutex_t inode_mutex;


// This is for releasing an inode number to inode cache
// Inode cache contains recently released inode numbers for reuse
static inline void
release_inode(uint64_t inode_num)
{
	pthread_mutex_lock(&inode_mutex);
	active_inodes --;
	pthread_mutex_unlock(&inode_mutex);

	return;
}

// This is for allocating new inode number

static inline unsigned long long
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


	return inode_number;
}


/*
 * del_inode -  Delete inode from DB and release the inode num to cache
 *
 * inode - in-core inode structure . Should be non-NULL
 * db - DB pointer. Should be non-NULL
 *
 * Returns 0 on success and negative number indicating error on failure.
 */


static int
del_inode(sstack_inode_t *inode, db_t *db)
{
	char inode_str[MAX_INODE_LEN] = { '\0' };
	char *data = NULL;
	int ret = -1;
	unsigned long long inode_num = 0;

	// Parameter validation
	if (NULL == inode || NULL == db) {
		sfs_log(db->ctx, SFS_ERR, "%s: Invalid parameters specified.\n",
						__FUNCTION__);
		return -EINVAL;
	}

	inode_num = inode->i_num;
	if (inode_num < INODE_NUM_START) {
		sfs_log(db->ctx, SFS_ERR, "%s: Invalid parameters specified.\n",
						__FUNCTION__);
		return -EINVAL;
	}

	sprintf(inode_str, "%lld", inode_num);

	if (db->db_ops.db_remove && ((db->db_ops.db_remove(inode_str,
					INODE_TYPE, db->ctx)) == 1)) {
		sfs_log(db->ctx, SFS_INFO, "%s: Succeeded for inode %lld \n",
				__FUNCTION__, inode_num);

		ret = 0;
	} else {
		sfs_log(db->ctx, SFS_ERR, "%s: Failed for inode %lld \n",
				__FUNCTION__, inode_num);

		ret = -2;
	}

	free(data);

	release_inode(inode->i_num);

	return ret;
}

#endif // __SFS_INTERNAL_H__
