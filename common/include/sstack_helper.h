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

#ifndef __SSTACK_HELPER_H_
#define __SSTACK_HELPER_H_

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <sstack_nfs.h>
#include <sstack_log.h>
#include <sstack_md.h>

#define MAX_INODE_LEN 40 // Maximum number of characters in 2^128 is 39

static inline char *
get_opt_str(const char *arg, char *opt_name)
{
	char *str = index(arg, '=');

	ASSERT((str), "Invalid parameter specified. Exiting...", 0, 0, 0);
	if (!str)
		exit(-1);

	str ++;
	str = strdup(str);
	ASSERT((str), "strdup failed. Exiting...", 0, 0, 0);
	if (!str)
		exit(-1);

	return str;
}

/*
 * fill_inode - Convert the flattened inode record into inode structure
 *
 * inode - Inode structure . Must be non-NULL.
 * data - record read from db
 * ctx - Log context
 *
 * Returns 0 on success and negative integer indicating error upon failure.
 *
 * Note:
 * This routine allocates following on heap:
 * 		i_erasure - a flattened sstack_erasure_t structure
 *				  - Each record has (length + data ptr) format;
 *		i_extent - array of ssatck_extent_t structure.
 * Caller needs to free in the following order after use:
 * In i_eraure case, path of each erasure code segment need to be freed
 * individually before freeing i_erasure
 * In i_extent case, e_path of each extent need to be freed before freeing
 * i_extent.
 */

static inline int
fill_inode(sstack_inode_t *inode, char *data, log_ctx_t *ctx)
{
	char *erasure = NULL;
	char *extents = NULL;
	char *temp = NULL;
	char *cur = NULL;
	sstack_inode_t *in = (sstack_inode_t *) data;
	int i = 0;
	int path_len = 0;
	int covered = 0;

	if (NULL == inode || NULL == data) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid parameters passed.\n", __FUNCTION__);

		return -EINVAL;
	}
	memcpy(inode, data, get_inode_fixed_fields_len());
	cur = (data + get_inode_fixed_fields_len());

	// Copy remaining fields 
	// 1. Copy extentded attributes
	temp = (char *) malloc(in->i_xattrlen);
	if (NULL == temp) {
		sfs_log(ctx, SFS_ERR, "%s: Unable to allocate memory \n",
						__FUNCTION__);
		return -ENOMEM;
	}
	memcpy((void *) temp, (void *) cur, in->i_xattrlen);
	cur += in->i_xattrlen;

	// 2. Erasure code segment paths
	// Copy remaining fields
	// 1. Erasure code segment paths
	for (i = 0; i < in->i_numerasure; i++) {
		sstack_erasure_t *er;

		er = (sstack_erasure_t *) ((char *) (data +
						get_inode_fixed_fields_len() + covered));

		memcpy(&path_len, (cur + covered), 4);
		temp = malloc(path_len);
		if (NULL == temp) {
			sfs_log(ctx, SFS_ERR, "%s: Unable to allocate memory \n",
					__FUNCTION__);
			return -ENOMEM;
		}
		// Copy current erasure code segment path
		memcpy(temp, (cur + covered + 4), path_len);
		// Assign it to current erasure code segment
		er->path = temp;
		erasure = realloc(erasure, (sizeof(sstack_erasure_t) + covered));
		if (NULL == erasure) {
			sfs_log(ctx, SFS_ERR, "%s: Unable to allocate memory \n",
					__FUNCTION__);
			return -ENOMEM;
		}
		memcpy((erasure + covered), er, sizeof(sstack_erasure_t));
		covered += (4 + path_len);
	}

	inode->i_erasure = (sstack_erasure_t *) erasure;

	// 3. Extent paths
	cur = (data + get_inode_fixed_fields_len() + covered);
	covered = 0;

	for (i = 0; i < in->i_numextents; i++) {
		sstack_extent_t *ex;
		char *path;

		ex = (sstack_extent_t *) ((char *)(cur + covered));
		path = malloc(ex->e_numreplicas * PATH_MAX);
		if(NULL == path) {
			sfs_log(ctx, SFS_ERR, "%s: Unable to allocate memory \n",
					__FUNCTION__);
			return -ENOMEM;
		}
		memcpy(path, (cur + get_extent_fixed_fields_len() + covered),
				ex->e_numreplicas * PATH_MAX);
		ex->e_path =  (sstack_file_handle_t *) path;
		extents = realloc(extents, (sizeof(sstack_extent_t) + covered));
		if (NULL == path) {
			sfs_log(ctx, SFS_ERR, "%s: Unable to allocate memory \n",
					__FUNCTION__);
			return -ENOMEM;
		}
		memcpy((extents + covered), ex, sizeof(sstack_extent_t));
		covered += (sizeof(sstack_extent_t) + (ex->e_numreplicas * PATH_MAX));
	}
	inode->i_extent = (sstack_extent_t *) extents;


	return 0;
}


/*
 * get_inode - Retrieve inode information from the db
 *
 * inode_num - Inode number of the inode to retrieve
 * inode - Inode structure to be filled and returned. Should not be NULL.
 * db - db pointer
 *
 * Gets inode structure from db and populate fields in inode.
 * Members like i_erasure and i_extents are allocated here. It is caller's
 * responsibility to free them after use.
 *
 * Returns 0 on success and a negative number indicating error on failure.
 */

static inline int
get_inode(unsigned long long inode_num, sstack_inode_t *inode, db_t *db)
{
	char inode_str[MAX_INODE_LEN] = { '\0' };
	char *data = NULL;

	// Parameter validation
	if (inode_num < INODE_NUM_START || NULL == inode || NULL == db) {
		sfs_log(db->ctx, SFS_ERR, "%s: Invalid parameters specified.\n",
						__FUNCTION__);
		return -EINVAL;
	}

	sprintf(inode_str, "%lld", inode_num);

	if (db->db_ops.db_get && (db->db_ops.db_get(inode_str, data,
					INODE_TYPE, db->ctx))) {
		int ret = -1;

		sfs_log(db->ctx, SFS_INFO, "%s: Succeeded for inode %lld \n",
				__FUNCTION__, inode_num);
		// Workhorse
		ret = fill_inode(inode, data, db->ctx);

		return ret;
	} else {
		sfs_log(db->ctx, SFS_ERR, "%s: Failed for inode %lld \n",
				__FUNCTION__, inode_num);
		return -1;
	}
}

/*
 * flatten_inode - Flatten inode structure for storing in DB
 *
 * inode - in-core inode structure
 * len - O/P parameter indicating record size
 * ctx - Log context
 *
 * Returns non-NULL buffer containing the record on success. Return NULL
 * upon failure and logs the reason for failure.
 */

static inline char *
flatten_inode(sstack_inode_t *inode, size_t *len, log_ctx_t *ctx)
{
	char *data = NULL;
	int fixed_len = 0;
	char *temp = 0;
	int i = 0;

	// Parameter validation
	if (NULL == inode) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid parameters passed \n", __FUNCTION__);

		return NULL;
	}
	*len = 0; // Just in case

	// Copy fixed fields of the inode
	fixed_len = get_inode_fixed_fields_len();
	data = malloc(fixed_len);
	if (NULL == data) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to allocate memory for "
			"storing fixed fields of inode %lld\n",  __FUNCTION__,
			inode->i_num);
		return NULL;
	}
	memcpy(data, inode, fixed_len);
	*len += fixed_len;
	// Copy remaining fields

	// 1. Extended attributes
	memcpy((void *) (data + (*len)), (void *) inode->i_xattr,
					inode->i_xattrlen);
	*len += inode->i_xattrlen;

	// 2. Erausre
	for (i = 0; i < inode->i_numerasure; i++) {
		sstack_erasure_t *er;

		er = inode->i_erasure;
		// Next 4 bytes contain path_len
		temp = realloc(data, ((*len) + sizeof(int)));
		if (NULL == temp) {
			sfs_log(ctx, SFS_ERR, "%s: Failed to allocate memory for "
				"storing fixed fields of inode %lld\n",  __FUNCTION__,
				inode->i_num);
			free(data); // Freeup old 
			return NULL;
		}
		data = temp;
		memcpy((void *) (data + (*len)), (void *) &er->path_len, sizeof(int));
		*len += sizeof(int);
		// Copy path
		temp = realloc(data, ((*len) + er->path_len));
		if (NULL == temp) {
			sfs_log(ctx, SFS_ERR, "%s: Failed to allocate memory for "
				"storing fixed fields of inode %lld\n",  __FUNCTION__,
				inode->i_num);
			free(data); // Freeup old 
			return NULL;
		}
		data = temp;
		memcpy((void *) (data + (*len)), er->path, er->path_len);
		*len += er->path_len;
		er ++;
	}

	// 3. Extents
	for (i = 0; i < inode->i_numextents; i++) {
		sstack_extent_t *ex;

		ex = inode->i_extent;
		fixed_len = get_extent_fixed_fields_len();
		// Copy fixed fields
		temp = realloc(data, ((*len) + fixed_len));
		if (NULL == temp) {
			sfs_log(ctx, SFS_ERR, "%s: Failed to allocate memory for "
				"storing fixed fields of inode %lld\n",  __FUNCTION__,
				inode->i_num);
			free(data); // Freeup old 
			return NULL;
		}
		data = temp;
		memcpy((void *) (data + (*len)), (void *) &ex->e_offset, fixed_len);
		*len += fixed_len;

		// Copy extent paths
		temp = realloc(data, ((*len) + (ex->e_numreplicas * PATH_MAX)));
		if (NULL == temp) {
			sfs_log(ctx, SFS_ERR, "%s: Failed to allocate memory for "
				"storing fixed fields of inode %lld\n",  __FUNCTION__,
				inode->i_num);
			free(data); // Freeup old 
			return NULL;
		}
		data = temp;
		memcpy((void *) (data + (*len)), ex->e_path,
						(ex->e_numreplicas * PATH_MAX));
		*len += (ex->e_numreplicas * PATH_MAX);
		ex ++;
	}

	return data;
}

/*
 * put_inode -  Store flattened inode structure onto db
 *
 * inode - in-core inode structure . Should be non-NULL
 * db - DB pointer. Should be non-NULL
 *
 * Returns 0 on success and negative number indicating error on failure.
 */


static inline int
put_inode(sstack_inode_t *inode, db_t *db)
{
	char inode_str[MAX_INODE_LEN] = { '\0' };
	char *data = NULL;
	size_t len = 0;
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

	data = flatten_inode(inode, &len, db->ctx);
	if (NULL == data) {
		sfs_log(db->ctx, SFS_ERR, "%s: Unable to flatten inode %lld\n",
				__FUNCTION__, inode_num);

		return -1;
	}

	if (db->db_ops.db_insert && ((db->db_ops.db_insert(inode_str, data, len,
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

	return ret;
}

/*
 * sstack_free_ersaure - Helper function to free up memory allocated
 * 						to i_sraure
 *
 * ctx - log_ctx
 * erasure - Memory to be freed. Must be non-NULL
 * num_erasure - Number of erasure code segments
 */

static inline void
sstack_free_erasure(log_ctx_t *ctx,
				sstack_erasure_t *erasure,
				int num_erasure)
{
	sstack_erasure_t *er;
	int i = 0;

	// Parameter validation
	if (NULL == erasure) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid parameter. "
					"Could indicate memory leak \n", __FUNCTION__);
		return;
	}

	er = erasure;

	while (i < num_erasure) {
		free(er->path);
		er ++;
		if (NULL == er) {
			sfs_log(ctx, SFS_ERR, "%s: FATAL ERROR. "
					"Could indicate corruption \n", __FUNCTION__);
			return;
		}
		i++;
	}

	free(erasure);
}

#endif //__SSTACK_HELPER_H_
