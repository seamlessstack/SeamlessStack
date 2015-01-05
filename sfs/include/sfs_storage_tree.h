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

#ifndef __SFS_STORAGE_TREE_H__                                             
#define __SFS_STORAGE_TREE_H__

#include <stdio.h>   
#include <rb.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <sstack_types.h>

#define STNODE_MAGIC 0xb3b3b3b3

typedef struct storage_node sfs_st_t;
struct storage_node {
	uint32_t	magic;
	rb_node(sfs_st_t) link;
	sstack_address_t address;
    char rpath[PATH_MAX];
    sfs_protocol_t access_protocol;
    sstack_storage_type_t type;
    size_t size;
};

static int
storage_tree_cmp(sfs_st_t *node1, sfs_st_t *node2)
{
	int ret = -1;
	int cmp = 0;

	// Parameter validation
    if (NULL == node1 || NULL == node2) {
        sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified \n",
			                        __FUNCTION__);
        return EINVAL;
    }

    // Check node validity
    if ((node1->magic != STNODE_MAGIC) || (node2->magic != STNODE_MAGIC)) {
        sfs_log(sfs_ctx, SFS_ERR, "%s: Corrupt nodes passed \n");
        return EFAULT;
    }

	cmp= memcmp(&(node1->address), &(node2->address), sizeof(sstack_address_t));
	(cmp > 0) ? (ret = 1) : ((cmp < 0) ? (ret = -1) : 0);
	if (ret == 0) {
		cmp = strcmp(node1->rpath, node2->rpath);
		(cmp > 0) ? (ret = 1) : ((cmp < 0) ? (ret = -1) : 0);
	}
	
	return (ret);
}

typedef rbt(sfs_st_t) storage_tree_t;
rb_gen(static, storage_tree_, storage_tree_t, sfs_st_t, link, storage_tree_cmp);
extern storage_tree_t *storage_tree;

typedef sfs_st_t *(*sfs_storage_fn)(storage_tree_t *, sfs_st_t *, void *);

static inline storage_tree_t *
storage_tree_init(void)
{
    storage_tree_t *tree = NULL;

    tree = (storage_tree_t *) malloc(sizeof(storage_tree_t));
    if (NULL == tree) {
        sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to allocate memory for storage "
			                        "tree.\n", __FUNCTION__);
        return NULL;
    }
    storage_tree_new(tree);

	return tree;
}

static inline sfs_st_t *
create_sttree_node(void)
{
    sfs_st_t *node = NULL;

    node = (sfs_st_t *) malloc(sizeof(sfs_st_t));
    if (NULL == node) {
        sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to allocate memory for storage "
				                        "tree node.\n", __FUNCTION__);
        return NULL;
    }

    return node;
}

static inline int
sfs_storage_node_insert(sstack_address_t address, char *path, 
		sfs_protocol_t proto, sstack_storage_type_t type, size_t size)
{
    sfs_st_t *node = NULL;

    node = create_sttree_node();
    if (node == NULL)
        return -1;

	node->magic = STNODE_MAGIC;
    node->address = address;
    strcpy(node->rpath, path);
    node->access_protocol = proto;
	node->type = type;
	node->size = size;

	storage_tree_insert(storage_tree, node);

	return (0);
}

static inline void
sfs_storage_node_remove(sstack_address_t address, char *path)
{
    sfs_st_t *node = NULL;
    sfs_st_t snode;

    snode.magic = STNODE_MAGIC;
    snode.address = address;
	strcpy(snode.rpath, path);

    node = storage_tree_search(storage_tree, &snode);

	if (NULL == node) {
        sfs_log(sfs_ctx, SFS_ERR, "%s: Node not present in "
                     "storage tree \n", __FUNCTION__);
        errno = EFAULT;
        return;
    }
    // Delete the node
    storage_tree_remove(storage_tree, node);
	free(node);
	return;
}

static inline void
sfs_storage_node_update(sstack_address_t address, char *path, size_t size)
{
    sfs_st_t *node = NULL;
    sfs_st_t snode;

    snode.magic = STNODE_MAGIC;
    snode.address = address;
	strcpy(snode.rpath, path);

    node = storage_tree_search(storage_tree, &snode);

	if (NULL == node) {
        sfs_log(sfs_ctx, SFS_ERR, "%s: Node not present in "
                     "storage tree \n", __FUNCTION__);
        errno = EFAULT;
        return;
    }

    // Delete the node
    storage_tree_remove(storage_tree, node);
	node->size = size;
	storage_tree_insert(storage_tree, node);
	
	return;
}

static inline void
sfs_storage_tree_iter(sfs_storage_fn cb)
{
	storage_tree_iter(storage_tree, NULL, cb, NULL);

	return;
}	

#endif // __SFS_STORAGE_TREE_H__	


