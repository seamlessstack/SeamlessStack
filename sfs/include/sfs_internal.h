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

#ifndef __SFS_INTERNAL_H__
#define __SFS_INTERNAL_H__

#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <sstack_jobs.h>
#include <bds_list.h>
#include <sstack_log.h>
#include <sstack_md.h>

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

/*
 * sstack_sfsd_pool_init - Initialize sfsd pool
 *
 * This function is called before sfsds are added to sfs by CLI.
 * This function initializes the pool list.
 *
 * Returns pointer to the pool list if successful. Otherwise returns NULL.
 */
static inline sstack_sfsd_pool_t *
sstack_sfsd_pool_init(void)
{
	sstack_sfsd_pool_t *pool = NULL;
	sstack_sfsd_pool_t *temp = NULL;
	int i = 0;

	// Allocate memory for pools
	pool = (sstack_sfsd_pool_t *) calloc(sizeof(sstack_sfsd_pool_t),
					MAX_SFSD_POOLS);
	if (NULL == pool) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to allocate memory for "
						"sfsd pools.\n", __FUNCTION__);

		return NULL;
	}

	// Initialize individual members
	temp = pool;
	for (i = 0; i < MAX_SFSD_POOLS; i++) {
		// Following 5 lines are to avoid compilation error
		// due to weird C99 issue. These are repeated elsewhere
		uint32_t low = *(uint32_t *) &weights[i][0];
		uint32_t high = *(uint32_t *) &weights[i][1];

		temp->weight_range.low = low;
		temp->weight_range.high = high;
		INIT_LIST_HEAD((bds_list_head_t) &temp->list);
		pthread_spin_init(&temp->lock, PTHREAD_PROCESS_PRIVATE);
		temp ++;
	}

	return pool;
}

/*
 * sstack_sfsd_pool_destroy - Destroy sfsd pool
 *
 * pools - pointer to global sfsd pool structure
 *
 * Called during exit to clean up the data strctures.
 */

static inline int
sstack_sfsd_pool_destroy(sstack_sfsd_pool_t *pools)
{
	int i = 0;
	sstack_sfsd_pool_t *temp = NULL;

	// Parameter validation
	if (NULL == pools) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameter specified \n",
						__FUNCTION__);
		errno = EINVAL;
		return -1;
	}

	temp = pools;
	for (i = 0; i < MAX_SFSD_POOLS; i++) {
		temp->weight_range.low = 0;
		temp->weight_range.high = 0;
		// TODO
		// Free all the entries in the list befoe reinitializing the list
		INIT_LIST_HEAD((bds_list_head_t) &temp->list);
		pthread_spin_destroy(&temp->lock);
		temp ++;
	}

	free(pools);

	return 0;
}

/*
 * sstack_sfsd_add - Add sfsd to appropriate sfsd_pool
 *
 * weight - relative weight of the sfsd
 * pools - Global sfsd pool
 * sfsd - sfsd to be added to the pool
 *
 * Returns 0 on success and -1 on failure.
 */


static inline int
sstack_sfsd_add(uint32_t weight, sstack_sfsd_pool_t *pools,
				sfsd_t *sfsd)
{
	sstack_sfsd_pool_t *temp = NULL;
	int i = 0;
	int index = -1;


	// Parameter validation
	if (weight < MAXIMUM_WEIGHT || weight > MINIMUM_WEIGHT ||
				NULL == pools || NULL == sfsd) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parametes specified \n",
						__FUNCTION__);

		errno = EINVAL;
		return -1;
	}

	// Get index of sfsd pool that covers the specified storeg weight
	for (i = 0; i < MAX_SFSD_POOLS; i++) {
		uint32_t low = *(uint32_t *) &weights[i][0];
		uint32_t high = *(uint32_t *) &weights[i][1];
		if (weight >= low && weight <= high)
			break;
	}
	index = i;
	temp = pools;

	for (i = 0; i < index; i++)
		temp ++;

	// Now temp points to the right pool
	pthread_spin_lock(&temp->lock);
	bds_list_add_tail((bds_list_head_t) &sfsd->list,
								(bds_list_head_t) &temp->list);
	pthread_spin_unlock(&temp->lock);

	return 0;
}

/*
 * sstack_sfsd_remove - Remove sfsd from sfsd pool
 *
 * wight - relative weight of sfsd
 * pools - Global sfsd pool
 * sfsd - sfsd to be removed
 *
 * This function should be called when the node running sfsd is decommissioned.
 *
 * Returns 0 on success and -1 on failure.
 */

static inline int
sstack_sfsd_remove(uint32_t weight, sstack_sfsd_pool_t *pools,
					sfsd_t *sfsd)
{
	bds_list_head_t head = NULL;
	sstack_sfsd_pool_t *temp = NULL;
	int found = -1;
	int index = -1;
	int i = 0;
	sfsd_t *s = NULL;

	// Parameter validation
	if (weight < MAXIMUM_WEIGHT || weight > MINIMUM_WEIGHT ||
				NULL == pools || NULL == sfsd) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parametes specified \n",
						__FUNCTION__);

		errno = EINVAL;
		return -1;
	}

	// Get index of sfsd pool that covers the specified storage weight
	for (i = 0; i < MAX_SFSD_POOLS; i++) {
		uint32_t low = *(uint32_t *) &weights[i][0];
		uint32_t high = *(uint32_t *) &weights[i][1];

		if (weight >= low && weight <= high)
			break;
	}
	index = i;
	temp = pools;

	for (i = 0; i < index; i++)
		temp ++;

	pthread_spin_lock(&temp->lock);
	if (list_empty((bds_list_head_t) &temp->list)) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Empty list at weight range %d %d. \n",
						__FUNCTION__, temp->weight_range.low,
						temp->weight_range.high);
		pthread_spin_unlock(&temp->lock);

		return -1;
	}

	pthread_spin_unlock(&temp->lock);
	head =(bds_list_head_t) &temp->list;
	pthread_spin_lock(&temp->lock);
	list_for_each_entry(s, head, list) {
		if (s == sfsd) {
			bds_list_del((bds_list_head_t) &s->list);
			found = 1;
			break;
		}
	}
	pthread_spin_unlock(&temp->lock);

	if (found != 1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: sfsd 0x%llx not found in sfsd pool. "
						"Posible list corruption detected\n", __FUNCTION__,
						sfsd);
		return -1;
	}

	return 0;
}

struct sfs_cache_entry {
	char name[CACHE_NAME_MAX];
	size_t size;
};

#endif // __SFS_INTERNAL_H__
