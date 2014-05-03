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

#ifndef __SFS_LOCK_TREE_H__
#define __SFS_LOCK_TREE_H__

#include <stdio.h>
#include <rb.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

#define LOCKNODE_MAGIC  0x28071937

typedef enum {
	RDLOCKED = 1,
	WRLOCKED = 2,
	UNLOCKED = 3,
} sstack_lock_state_t;
	

// RB-tree node for storing locks for the files

typedef struct sstack_file_lock sstack_file_lock_t;
struct sstack_file_lock {
	uint32_t magic;
	rb_node(sstack_file_lock_t) link;
	unsigned long long inode_num;
	sstack_lock_state_t state;
	pthread_spinlock_t lock; // To protect state field
};

extern pthread_spinlock_t filelock_lock;

// FUNCTIONS

/*
 * filelock_tree_cmp - file lock tree comparison function
 *
 * node1, node2 - tree nodes to be compared based on inode_num
 *
 * Returns either 1 or -1 on success. Any value greater than 1 are errors
 */

static inline int
filelock_tree_cmp(sstack_file_lock_t *node1, sstack_file_lock_t *node2)
{
	int ret = -1;

	// Parameter validation
	if (NULL == node1 || NULL == node2) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);
		return EINVAL;
	}

	// Check node validity
	if ((node1->magic != LOCKNODE_MAGIC) || (node2->magic != LOCKNODE_MAGIC)) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Corrupt nodes passed \n");
		return EFAULT;
	}

	ret = (node1->inode_num > node2->inode_num) - (node1->inode_num <
					node2->inode_num);

	return ret;
}

typedef rbt(sstack_file_lock_t) filelock_tree_t;
rb_gen(static, filelock_tree_, filelock_tree_t, sstack_file_lock_t, link, filelock_tree_cmp);
extern filelock_tree_t *filelock_tree;

/*
 * filelock_tree_init - Initialize filelock tree
 *
 * Returns newly intialized jobtree upon success and returns NULL upon failure
 */

static inline filelock_tree_t *
filelock_tree_init(void)
{
	filelock_tree_t *tree = NULL;

	tree = (filelock_tree_t *) malloc(sizeof(filelock_tree_t));
	if (NULL == tree) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to allocate memory for filelock "
						"tree.\n", __FUNCTION__);
		return NULL;
	}
	filelock_tree_new(tree);
	sfs_log(sfs_ctx, SFS_INFO, "%s: tree = 0x%x \n", __FUNCTION__, tree);

	return tree;
}

/*
 * create_filelocktree_node - Create a new node for filelock tree
 *
 * Returns newly allocated node upon success and NULL upon failure.
 */

static sstack_file_lock_t *
create_filelocktree_node(void)
{
	sstack_file_lock_t *node = NULL;

	node = (sstack_file_lock_t *) malloc(sizeof(sstack_file_lock_t));
	if (NULL == node) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to allocate memory for filelock "
						"tree node.\n", __FUNCTION__);
		return NULL;
	}
	node->magic = LOCKNODE_MAGIC;
	node->state = UNLOCKED;
	pthread_spin_init(&node->lock, PTHREAD_PROCESS_PRIVATE);

	return node;
}

/*
 * sfs_filelock_context_insert - Insert filelock context for inode into RB-tree
 *
 * inode_num - Inode number of the file
 *
 * Returns 0 on success and -1 on failure
 */

static inline int
sfs_filelock_context_insert(unsigned long long inode_num)
{
	sstack_file_lock_t *node = NULL;

	// Parameter validation
	if (inode_num < 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameter specified \n",
						__FUNCTION__);
		errno = EINVAL;

		return -1;
	}

	node = create_filelocktree_node();
	if (node == NULL)
		return -1;

	node->inode_num = inode_num;

	pthread_spin_lock(&filelock_lock);	
	filelock_tree_insert(filelock_tree, node);
	pthread_spin_unlock(&filelock_lock);	

	return 0;
}


/*
 * sfs_filelock_context_remove - Remove RB-tree node for the inode
 *
 * inode_num - inode number of the file
 *
 * Appropriate errno is set upon failure.
 */
static inline void
sfs_filelock_context_remove(unsigned long long inode_num)
{
	sstack_file_lock_t *node = NULL;
	sstack_file_lock_t snode;

	// Parameter validation
	if (inode_num < 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameter specified \n",
						__FUNCTION__);
		errno = EINVAL;

		return;
	}

	snode.magic = LOCKNODE_MAGIC;
	snode.inode_num = inode_num;

	pthread_spin_lock(&filelock_lock);	
	node = filelock_tree_search(filelock_tree, &snode);
	if (NULL == node) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Node with key %lld not present in "
						"filelock tree \n", __FUNCTION__, inode_num);

		errno = EFAULT;
		pthread_spin_unlock(&filelock_lock);	

		return;
	}

	// Delete the node
	filelock_tree_remove(filelock_tree, node);
	pthread_spin_unlock(&filelock_lock);	
	errno = 0;

	return;
}

/*
 * sfs_rdlock - Obtain read lock on inode
 * inode_num - inode number of the file to be locked
 *
 * Returns 0 on success and -1 upon failure with appropriate error values.
 */

static inline int
sfs_rdlock(unsigned long long inode_num)
{
	sstack_file_lock_t node;
	sstack_file_lock_t *file_lock = NULL;

	// Parameter validation
	if (inode_num < 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameter specified \n",
					__FUNCTION__);
		errno = EINVAL;

		return -1;
	}
	// Obtain the file lock structure for the inode
	node.magic = LOCKNODE_MAGIC;
	node.inode_num = inode_num;
	pthread_spin_lock(&filelock_lock);	
	file_lock = filelock_tree_search(filelock_tree, &node);
	if (NULL == file_lock) {
		// No existing locks for the file
		// Obtain read lock and return success

		file_lock = create_filelocktree_node();
		if (file_lock == NULL) {
			sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to allocate memory for "
				"new file lock node for inode %lld \n", __FUNCTION__,
				inode_num);
			errno = ENOMEM;
			pthread_spin_unlock(&filelock_lock);

			return -1;
		}
		file_lock->inode_num = inode_num;

		pthread_spin_lock(&file_lock->lock);
		file_lock->state = RDLOCKED;
		pthread_spin_unlock(&file_lock->lock);
		// Update the lock information in RB-tree
		filelock_tree_insert(filelock_tree, file_lock);

		sfs_log(sfs_ctx, SFS_INFO, "%s: Successfully obtained read lock "
					"for inode %lld \n", __FUNCTION__, inode_num);
		pthread_spin_unlock(&filelock_lock);
		return 0;
	} else {
		// Prior lock information exists
		pthread_spin_lock(&file_lock->lock);
		switch(file_lock->state) {
			case RDLOCKED:
				// File is already read locked
				// Simply return success as parallel readers are allowed
				// in the absense of a writer.
				pthread_spin_unlock(&file_lock->lock);
				sfs_log(sfs_ctx, SFS_INFO, "%s: Successfully obtained read "
					"lock for inode %lld \n", __FUNCTION__, inode_num);
				pthread_spin_unlock(&filelock_lock);
				return 0;
			case WRLOCKED:
				// File is already write locked.
				// Return error as readers are not allowed when writer is
				// present
				errno = EAGAIN;
				pthread_spin_unlock(&file_lock->lock);
				sfs_log(sfs_ctx, SFS_ERR, "%s: Inode %lld already write "
					"locked. Please try later \n", __FUNCTION__, inode_num);
				pthread_spin_unlock(&filelock_lock);
				return -1;
			case UNLOCKED:
				// Obtain the read lock and update tree
				file_lock->state = RDLOCKED;
				pthread_spin_unlock(&file_lock->lock);
				filelock_tree_insert(filelock_tree, file_lock);
				sfs_log(sfs_ctx, SFS_INFO, "%s: Successfully obtained read "
					"lock for inode %lld \n", __FUNCTION__, inode_num);
				pthread_spin_unlock(&filelock_lock);
				return 0;
			default:
				sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid file lock state. "
					"INDICATES MEMORY CORRUPTION!!! \n", __FUNCTION__);
				pthread_spin_lock(&file_lock->lock);
				pthread_spin_unlock(&filelock_lock);
				return -1;
		}

	}
}


/*
 * sfs_wrlock - Obtain write lock on inode
 * inode_num - inode number of the file to be locked
 *
 * Returns 0 on success and -1 upon failure with appropriate error values.
 */

static inline int
sfs_wrlock(unsigned long long inode_num)
{
	sstack_file_lock_t node;
	sstack_file_lock_t *file_lock = NULL;

	// Parameter validation
	if (inode_num < 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameter specified \n",
					__FUNCTION__);
		errno = EINVAL;

		return -1;
	}
	// Obtain the file lock structure for the inode
	node.magic = LOCKNODE_MAGIC;
	node.inode_num = inode_num;
	pthread_spin_lock(&filelock_lock);
	file_lock = filelock_tree_search(filelock_tree, &node);
	if (NULL == file_lock) {
		// No existing locks for the file
		// Obtain write lock and return success

		file_lock = create_filelocktree_node();
		if (file_lock == NULL) {
			sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to allocate memory for "
				"new file lock node for inode %lld \n", __FUNCTION__,
				inode_num);
			errno = ENOMEM;
			pthread_spin_unlock(&filelock_lock);

			return -1;
		}
		file_lock->inode_num = inode_num;
		pthread_spin_lock(&file_lock->lock);
		file_lock->state = WRLOCKED;
		pthread_spin_unlock(&file_lock->lock);
		// Update the lock information in RB-tree
		filelock_tree_insert(filelock_tree, file_lock);

		sfs_log(sfs_ctx, SFS_INFO, "%s: Successfully obtained write lock "
					"for inode %lld \n", __FUNCTION__, inode_num);
		pthread_spin_unlock(&filelock_lock);
		return 0;
	} else {
		// Prior lock information exists
		pthread_spin_lock(&file_lock->lock);
		switch(file_lock->state) {
			case RDLOCKED:
				// File is already read locked.
				// Return error as writer is not allowed when readers are
				// present
				errno = EAGAIN;
				pthread_spin_unlock(&file_lock->lock);
				sfs_log(sfs_ctx, SFS_ERR, "%s: Inode %lld already read "
					"locked. Please try later \n", __FUNCTION__, inode_num);
				pthread_spin_unlock(&filelock_lock);
				return -1;
			case WRLOCKED:
				// File is already write locked.
				// Return error as parallel writers are recipe for disaster
				errno = EAGAIN;
				pthread_spin_unlock(&file_lock->lock);
				sfs_log(sfs_ctx, SFS_ERR, "%s: Inode %lld already write "
					"locked. Please try later \n", __FUNCTION__, inode_num);
				pthread_spin_unlock(&filelock_lock);
				return -1;
			case UNLOCKED:
				// Obtain the write lock and update tree
				file_lock->state = WRLOCKED;
				pthread_spin_unlock(&file_lock->lock);
				filelock_tree_insert(filelock_tree, file_lock);
				sfs_log(sfs_ctx, SFS_INFO, "%s: Successfully obtained write "
					"lock for inode %lld \n", __FUNCTION__, inode_num);
				pthread_spin_unlock(&filelock_lock);
				return 0;
			default:
				sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid file lock state. "
					"INDICATES MEMORY CORRUPTION!!! \n", __FUNCTION__);
				pthread_spin_lock(&file_lock->lock);
				pthread_spin_unlock(&filelock_lock);
				return -1;
		}

	}
}

/*
 * sfs_unlock - Unlock the inode lock
 * inode_num - inode number of the file to be unlocked
 *
 * Returns 0 on success and -1 upon failure.
 */

static inline int
sfs_unlock(unsigned long long inode_num)	
{
	sstack_file_lock_t node;
	sstack_file_lock_t *file_lock = NULL;

	// Parameter validation
	if (inode_num < 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameter specified \n",
				__FUNCTION__);

		errno = EINVAL;
		return -1;
	}
	// Obtain the file lock structure for the inode
	node.magic = LOCKNODE_MAGIC;
	node.inode_num = inode_num;
	pthread_spin_lock(&filelock_lock);
	file_lock = filelock_tree_search(filelock_tree, &node);
	if (NULL == file_lock) {
		// There is not node for this inode in tree
		// This indicates file was never locked.
		// Unlock on a already unlocked file in an error.
		sfs_log(sfs_ctx, SFS_ERR, "%s: Inode %lld already unlocked. "
				"Invalid operation \n", __FUNCTION__, inode_num);

		errno = ENOENT;
		pthread_spin_unlock(&filelock_lock);
		return -1;
	}

	pthread_spin_lock(&file_lock->lock);
	file_lock->state = UNLOCKED;
	pthread_spin_unlock(&file_lock->lock);
//	filelock_tree_insert(filelock_tree, file_lock);
	filelock_tree_remove(filelock_tree, file_lock);
	sfs_log(sfs_ctx, SFS_INFO, "%s: Successfully unlocked inode %lld \n",
			 __FUNCTION__, inode_num);
	pthread_spin_unlock(&filelock_lock);

	return 0;
}

#endif // __SFS_LOCK_TREE_H__
