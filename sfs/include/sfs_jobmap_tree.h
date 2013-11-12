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

#ifndef __SFS_JOBMAP_TREE_H__
#define __SFS_JOBMAP_TREE_H__

#include <stdio.h>
#include <rb.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

#define JMNODE_MAGIC 0x5a5a5a5a

// RB-tree node for storing jobmap to thread id mapping
typedef struct jobmap_tree_node  sstack_jm_t;
struct jobmap_tree_node {
	uint32_t magic;
	rb_node(sstack_jm_t) link;
	pthread_t thread_id; // Key
	sstack_job_map_t *job_map;
};
typedef rbt(sstack_jm_t) jobmap_tree_t;
static inline int jobmap_tree_cmp(sstack_jm_t *node1, sstack_jm_t *node2);

rb_gen(static, jobmap_tree_, jobmap_tree_t, sstack_jm_t, link, jobmap_tree_cmp);

// BSS

extern jobmap_tree_t *jobmap_tree;

// FUNCTIONS

/*
 * jobmap_tree_cmp - Jobmap tree comparison function
 *
 * node1, node2 - tree nodes to be compared based on thread_ids
 *
 * Returns either 1 or -1 on success. Any value greater than 1 are errors
 */

static inline int
jobmap_tree_cmp(sstack_jm_t *node1, sstack_jm_t *node2)
{
	int ret = -1;

	// Parameter validation
	if (NULL == node1 || NULL == node2) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);
		return EINVAL;
	}

	// Check node validity
	if ((node1->magic != JMNODE_MAGIC) || (node2->magic != JMNODE_MAGIC)) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Corrupt nodes passed \n");
		return EFAULT;
	}

	ret = (node1->thread_id > node2->thread_id) - (node1->thread_id <
					node2->thread_id);
	if (ret == 0) {
		// Duplicates are not allowed in the tree, so force an arbitrary
		// ordering for non-identical items with equal keys.
		ret = (((uintptr_t) node1) > ((uintptr_t) node2)) -
				(((uintptr_t) node1) < ((uintptr_t) node2));
	}

	return ret;
}

/*
 * jobmap_tree_init - Initialize jobmap tree
 *
 * Returns newly intialized jobtree upon success and returns NULL upon failure
 */

static inline sstack_jm_t *
jobmap_tree_init(void)
{
	jobmap_tree_t *tree = NULL;

	tree = (jobmap_tree_t *) malloc(sizeof(jobmap_tree_t));
	if (NULL == tree) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to allocate memory for jobmap "
						"tree.\n", __FUNCTION__);
		return NULL;
	}
	jobmap_tree_new(tree);

	return tree;
}

/*
 * create_jmtree_node - Create a new node for jobmap tree
 *
 * Returns newly allocated node upon success and NULL upon failure.
 */

static inline sstack_jm_t *
create_jmtree_node(void)
{
	sstack_jm_t *node = NULL;

	node = (sstack_jm_t *) malloc(sizeof(sstack_jm_t));
	if (NULL == node) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to allocate memory for jobmap "
						"tree node.\n", __FUNCTION__);
		return NULL;
	}

	return node;
}

/*
 * sfs_job_context_insert - Insert thread_id to jobmap mapping into
 *							RB-tree
 *
 * thread_id - Thread id of the application thread
 * job_map - Jobmap structure corresponding to the job that application
 *			thread issued
 *
 * Returns 0 on success and -1 on failure
 */

static inline int
sfs_job_context_insert(pthread_t thread_id, sstack_job_map_t *job_map)
{
	sstack_jm_t *node = NULL;

	// Parameter validation
	if (thread_id < 0 || NULL == job_map) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);
		errno = EINVAL;

		return -1;
	}

	node = create_jmtree_node();
	if (node == NULL)
		return -1;

	node->magic = JMNODE_MAGIC;
	node->thread_id = thread_id;
	node->job_map = job_map;
	jobmap_tree_insert(jobmap_tree, node);

	return 0;
}


/*
 * sfs_job_context_remove - Remove RB-tree node for thread
 *
 * thread_id - Thread id of the application thread
 *
 * Appropriate errno is set upon failure.
 */
static inline void
sfs_job_context_remove(pthread_t thread_id)
{
	sstack_jm_t *node = NULL;

	// Parameter validation
	if (thread_id < 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);
		errno = EINVAL;

		return;
	}

	node = jobmap_tree_search(jobmap_tree, thread_id);
	if (NULL == node) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Node with key %d not present in "
						"jobmap tree \n", __FUNCTION__, (int) thread_id);

		errno = EFAULT;

		return;
	}

	// Delete the node
	jobmap_tree_remove(jobmap_tree, node);
	errno = 0;

	return;
}

#endif // __SFS_JOBMAP_TREE_H__
