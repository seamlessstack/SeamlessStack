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

#ifndef __SFS_JOBID_TREE_H__
#define __SFS_JOBID_TREE_H__

#include <stdio.h>
#include <rb.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <sfs_job.h>

#define JTNODE_MAGIC 0xa5a5a5a5

// RB-tree node for storing thread_is to job_id mapping
typedef struct job2thread_tree sstack_jt_t;
struct job2thread_tree {
	uint32_t magic;
	rb_node(sstack_jt_t) link;
	sstack_job_id_t job_id; // Key
	pthread_t thread_id;
	sfs_job_t *job;	
};

extern pthread_spinlock_t jobid_lock;

// BSS


// FUNCTIONS

/*
 * jobid_tree_cmp - Jobid tree comparison function
 *
 * node1, node2 - tree nodes to be compared based on thread_ids
 *
 * Returns either 1 or -1 on success. Any value greater than 1 are errors
 */
static int
jobid_tree_cmp(sstack_jt_t *node1, sstack_jt_t *node2)
{
	int ret = -1;

	// Parameter validation
	if (NULL == node1 || NULL == node2) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);
		return EINVAL;
	}

	// Check node validity
	if ((node1->magic != JTNODE_MAGIC) || (node2->magic != JTNODE_MAGIC)) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Corrupt nodes passed \n");
		return EFAULT;
	}

	ret = (node1->job_id > node2->job_id) - (node1->job_id < node2->job_id);
	if (ret == 0) {
		// Duplicates are not allowed in the tree, so force an arbitrary
		// ordering for non-identical items with equal keys.
		ret = (((uintptr_t) node1) > ((uintptr_t) node2)) -
				(((uintptr_t) node1) < ((uintptr_t) node2));
	}

	return ret;
}

typedef rbt(sstack_jt_t) jobid_tree_t;
rb_gen(static, jobid_tree_, jobid_tree_t, sstack_jt_t, link, jobid_tree_cmp);
extern jobid_tree_t *jobid_tree;


/*
 * jobid_tree_init - Initialize jobid tree
 *
 * Returns newly intialized jobid tree upon success and returns NULL
 * upon failure
 */

static inline jobid_tree_t *
jobid_tree_init(void)
{
	jobid_tree_t *tree = NULL;

	tree = (jobid_tree_t *) malloc(sizeof(jobid_tree_t));
	if (NULL == tree) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to allocate memory for jobmap "
						"tree.\n", __FUNCTION__);
		return NULL;
	}
	jobid_tree_new(tree);

	return tree;
}

/*
 * create_jttree_node - Create a new node for jobmap tree
 *
 * Returns newly allocated node upon success and NULL upon failure.
 */

static inline sstack_jt_t *
create_jttree_node(void)
{
	sstack_jt_t *node = NULL;

	node = (sstack_jt_t *) malloc(sizeof(sstack_jt_t));
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
sfs_job2thread_map_insert(pthread_t thread_id, sstack_job_id_t job_id)
{
	sstack_jt_t *node = NULL;

	// Parameter validation
	if (thread_id < 0 || job_id < 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);
		errno = EINVAL;

		return -1;
	}

	node = create_jttree_node();
	if (node == NULL)
		return -1;

	node->magic = JTNODE_MAGIC;
	node->thread_id = thread_id;
	node->job_id = job_id;
	pthread_spin_lock(&jobid_lock);
	jobid_tree_insert(jobid_tree, node);
	pthread_spin_unlock(&jobid_lock);

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
sfs_job2thread_map_remove(sstack_job_id_t job_id)
{
	sstack_jt_t *node = NULL;
	sstack_jt_t snode;

	// Parameter validation
	if (job_id < 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);
		errno = EINVAL;

		return;
	}
	snode.magic = JTNODE_MAGIC;
	snode.job_id = job_id;

	pthread_spin_lock(&jobid_lock);
	node = jobid_tree_search(jobid_tree, &snode);
	if (NULL == node) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Node with key %d not present in "
						"jobmap tree \n", __FUNCTION__, (int) job_id);

		errno = EFAULT;
		pthread_spin_unlock(&jobid_lock);

		return;
	}

	// Delete the node
	jobid_tree_remove(jobid_tree, node);
	pthread_spin_unlock(&jobid_lock);
	errno = 0;

	return;
}

#endif // __SFS_JOBID_TREE_H__
