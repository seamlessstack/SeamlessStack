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
#include <sstack_jobs.h>
#include <sstack_db.h>
#include <sstack_helper.h>
#include <sstack_md.h>
#include <bds_slab.h>
#include <sstack_transport.h>

// This file contains all the helper functions common for sfs and sfsd
static bds_cache_desc_t inode_cache = NULL;
static log_ctx_t *alloc_ctx = NULL;
/* Inode create functions */
int32_t sstack_helper_init(log_ctx_t *ctx)
{
	bds_status_t ret = -1;
	ret = bds_cache_create("helper-inode-cahe", sizeof(sstack_inode_t),
						   0, NULL, NULL, &inode_cache);
	if (ret != 0)
		sfs_log(ctx, SFS_ERR, "%s() - Cannot create inode cache\n",
				__FUNCTION__);
	else
		sfs_log(ctx, SFS_INFO, "%s() - Helper cache create successful\n",
				__FUNCTION__);
	alloc_ctx = ctx;
	return ret;
}

sstack_inode_t *sstack_create_inode(void)
{
	sstack_inode_t *inode = NULL;
	inode = (sstack_inode_t *) bds_cache_alloc(inode_cache);
	if (inode == NULL)
		sfs_log(alloc_ctx, SFS_ERR, "%s() - inode create returned NULL\n",
				__FUNCTION__);
	if (inode != NULL)
		memset(inode, 0, sizeof(*inode));
	return inode;
}

void sstack_free_inode(sstack_inode_t *inode)
{
	bds_cache_free(inode_cache, inode);
}




/* DB helper functions */

inline db_t *
create_db(void)
{
	return malloc(sizeof(db_t));
}

inline void
destroy_db(db_t * db)
{
	if (db)
		free(db);
}

inline void
db_register(db_t *db, db_init_t db_init, db_open_t db_open,
	db_close_t db_close, db_insert_t db_insert, db_remove_t db_remove,
	db_iterate_t db_iterate, db_get_t db_get, db_seekread_t db_seekread,
	db_update_t db_update, db_delete_t db_delete, db_cleanup_t db_cleanup,
	log_ctx_t *ctx)
{
	sfs_log(ctx, SFS_DEBUG, "%s: db = 0x%x \n", __FUNCTION__, db);
	if (db) {
		db->db_ops.db_init = db_init;
		db->db_ops.db_open = db_open;
		db->db_ops.db_close = db_close;
		db->db_ops.db_insert = db_insert;
		db->db_ops.db_remove = db_remove;
		db->db_ops.db_iterate = db_iterate;
		db->db_ops.db_get = db_get;
		db->db_ops.db_seekread = db_seekread;
		db->db_ops.db_update = db_update;
		db->db_ops.db_delete = db_delete;
		db->db_ops.db_cleanup = db_cleanup;
		db->ctx = ctx;
	}
}

/* Inode helper functions */
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
 * 		i_erasure - a flattened sstack_extent_t structure
 *				  - Each record has (length + data ptr) format;
 *		i_extent - array of ssatck_extent_t structure.
 * Caller needs to free in the following order after use:
 * In i_eraure case, path of each erasure code segment need to be freed
 * individually before freeing i_erasure
 * In i_extent case, e_path of each extent need to be freed before freeing
 * i_extent.
 */

static int
fill_inode(sstack_inode_t *inode, char **data, log_ctx_t *ctx)
{
	char *erasure = NULL;
	char *extents = NULL;
	char *temp = NULL;
	char *cur = NULL;
	sstack_inode_t *in = (sstack_inode_t *) *data;
	int i = 0;
	int path_len = 0;
	int covered = 0;
	sstack_extent_t *er = NULL;

	if (NULL == inode || NULL == *data) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid parameters passed.\n",
				__FUNCTION__);

		return -EINVAL;
	}
	sfs_log(ctx, SFS_DEBUG, "%s() - Fixed field len: %d\n", __FUNCTION__,
			get_inode_fixed_fields_len());
	memcpy(inode, *data, get_inode_fixed_fields_len());
	cur = (*data + get_inode_fixed_fields_len());

	// Copy remaining fields
	// 1. Copy extentded attributes
	if (in->i_xattrlen > 0) {
		temp = (char *) malloc(in->i_xattrlen);
		if (NULL == temp) {
			sfs_log(ctx, SFS_ERR, "%s: Unable to allocate memory \n",
					__FUNCTION__);
			return -ENOMEM;
		}
		memcpy((void *) temp, (void *) cur, in->i_xattrlen);
		cur += in->i_xattrlen;
	}

	// 2. Erasure code segment paths
	// Copy remaining fields
	// 1. Erasure code segment paths
	er = (sstack_extent_t *) ((char *) (*data +
				get_inode_fixed_fields_len() + covered));

	sfs_log(ctx, SFS_DEBUG, "%s() - numerasure: %d\n", __FUNCTION__,
			in->i_numerasure);
	for (i = 0; i < in->i_numerasure; i++) {
		temp = calloc(sizeof(sstack_file_handle_t), 1);
		if (NULL == temp) {
			sfs_log(ctx, SFS_ERR, "%s: Unable to allocate memory \n",
					__FUNCTION__);
			return -ENOMEM;
		}
		// Copy current erasure code segment path
		memcpy(temp, cur + covered , sizeof(sstack_file_handle_t));
		// Assign it to current erasure code segment
		er->e_path = (sstack_file_handle_t *) temp;
		covered += sizeof(sstack_file_handle_t);
		// Copy over cksum
		memcpy(&er->e_cksum, (cur + covered), sizeof(sstack_cksum_t));
		covered += sizeof(sstack_cksum_t);

		temp = realloc(erasure, (sizeof(sstack_extent_t) + covered));
		if (NULL == temp) {
			sfs_log(ctx, SFS_ERR, "%s: Unable to allocate memory \n",
					__FUNCTION__);
			free(temp);
			free(erasure);
			return -ENOMEM;
		}
		erasure = temp;
		memcpy((erasure + covered), er, sizeof(sstack_extent_t));
		covered += (4 + path_len);
	}

	inode->i_erasure = (sstack_extent_t *) erasure;

	// 3. Extent paths
	cur = (*data + get_inode_fixed_fields_len() + covered);
	covered = 0;

	sfs_log(ctx, SFS_DEBUG, "%s() - numextents: %d\n", __FUNCTION__,
			in->i_numextents);
	for (i = 0; i < in->i_numextents; i++) {
		sstack_extent_t *ex = NULL;
		char *path = NULL;

		ex = (sstack_extent_t *) ((char *)(cur + covered));
		path = malloc(ex->e_numreplicas * sizeof(sstack_file_handle_t));
		if(NULL == path) {
			sfs_log(ctx, SFS_ERR, "%s: Unable to allocate memory \n",
					__FUNCTION__);
			free(inode->i_erasure);
			return -ENOMEM;
		}
		memcpy(path, (cur + get_extent_fixed_fields_len() + covered),
				ex->e_numreplicas * sizeof(sstack_file_handle_t));
		ex->e_path =  (sstack_file_handle_t *) path;
		extents = realloc(extents, (sizeof(sstack_extent_t) + covered));
		if (NULL == path) {
			sfs_log(ctx, SFS_ERR, "%s: Unable to allocate memory \n",
					__FUNCTION__);
			free(inode->i_erasure);
			free(path);
			return -ENOMEM;
		}
		memcpy((extents + covered), ex, sizeof(sstack_extent_t));
		covered += (sizeof(sstack_extent_t) +
						(ex->e_numreplicas * sizeof(sstack_file_handle_t)));
	}
	inode->i_extent = (sstack_extent_t *) extents;

	// 4. sfsd info
	cur = (*data + get_inode_fixed_fields_len() + covered);
	covered = 0;

	inode->i_sfsds = (sstack_sfsd_info_t *)
		malloc(sizeof(sstack_sfsd_info_t *) * inode->i_numclients);
	if (NULL == inode->i_sfsds) {
		sfs_log(ctx, SFS_ERR, "%s: Unable to allocate memory \n",
						__FUNCTION__);
		free(inode->i_erasure);
		// TODO
		// Walk through i_extent and free each path
		free(inode->i_extent);
		return -ENOMEM;
	}
	// i_numclients includes primary sfsd
	// Copy i_numclients - 1 sfsd pointers as primary sfsd is already part
	// of the fixed fields
	if (inode->i_numclients > 1)
		memcpy((void *) inode->i_sfsds, (void *) cur,
				sizeof(sstack_sfsd_info_t *) * (inode->i_numclients - 1));

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

int
get_inode(unsigned long long inode_num, sstack_inode_t *inode, db_t *db)
{
	char inode_str[MAX_INODE_LEN] = { '\0' };
	char *data;

	// Parameter validation
	if (inode_num < INODE_NUM_START || NULL == inode || NULL == db) {
		sfs_log(db->ctx, SFS_ERR, "%s: Invalid parameters specified.\n",
						__FUNCTION__);
		return -EINVAL;
	}

	sprintf(inode_str, "%lld", inode_num);

	if (db->db_ops.db_get && (db->db_ops.db_get(inode_str, &data,
					INODE_TYPE, db->ctx))) {
		int ret = -1;

		sfs_log(db->ctx, SFS_INFO, "%s: Succeeded for inode %lld \n",
				__FUNCTION__, inode_num);
		// Workhorse
		ret = fill_inode(inode, &data, db->ctx);

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

static char *
flatten_inode(sstack_inode_t *inode, size_t *len, log_ctx_t *ctx)
{
	char *data = NULL;
	int fixed_len = 0;
	char *temp = 0;
	int i = 0;
	sstack_extent_t *er = NULL;
	sstack_extent_t *ex = NULL;

	// Parameter validation
	if (NULL == inode) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid parameters passed \n", __FUNCTION__);

		return NULL;
	}
	sfs_log(ctx, SFS_DEBUG, "%s() - %d\n", __FUNCTION__,__LINE__);
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
	sfs_log(ctx, SFS_DEBUG, "%s() - %d\n", __FUNCTION__,__LINE__);
	memcpy(data, inode, fixed_len);
	*len += fixed_len;
	// Copy remaining fields

	// 1. Extended attributes

	if (inode->i_xattrlen > 0) {
		memcpy((void *) (data + (*len)), (void *) inode->i_xattr,
					inode->i_xattrlen);
		*len += inode->i_xattrlen;
	}

	er = inode->i_erasure;
	sfs_log(ctx, SFS_DEBUG, "%s() - %d\n", __FUNCTION__,__LINE__);
	// 2. Erausre
	for (i = 0; i < inode->i_numerasure; i++) {
		// First field is e_path
		temp = realloc(data, ((*len) + sizeof(sstack_file_handle_t) +
					sizeof(sstack_cksum_t)));
		if (NULL == temp) {
			sfs_log(ctx, SFS_ERR, "%s: Failed to allocate memory for "
				"storing e_path of inode %lld\n",  __FUNCTION__,
				inode->i_num);
			free(data); // Freeup old
			return NULL;
		}
		data = temp;
		memcpy((void *) (data + (*len)), (void *) er->e_path,
				sizeof(sstack_file_handle_t));
		*len += sizeof(sstack_file_handle_t);
		// Copy cksum
		memcpy((void *) (data + (*len)), (void *)&er->e_cksum,
				sizeof(sstack_extent_t));
		*len += sizeof(sstack_extent_t);
		er ++;
	}

	sfs_log(ctx, SFS_DEBUG, "%s() - %d\n", __FUNCTION__,__LINE__);
	// 3. Extents

	ex = inode->i_extent;
	if(ex)
		sfs_log(ctx, SFS_DEBUG, "%s() - extents: %d numreplicas: %d\n",
				__FUNCTION__, inode->i_numextents, ex->e_numreplicas);
	for (i = 0; i < inode->i_numextents; i++) {

		fixed_len = get_extent_fixed_fields_len();
		// Copy fixed fields
		temp = realloc(data, ((*len) + fixed_len));
		if (NULL == temp) {
			sfs_log(ctx, SFS_ERR, "%s: Failed to allocate memory for "
				"storing fixed fields of extent %lld\n",  __FUNCTION__,
				inode->i_num);
			free(data); // Freeup old
			return NULL;
		}
		data = temp;
		memcpy((void *) (data + (*len)), (void *) &ex->e_offset, fixed_len);
		*len += fixed_len;

		// Copy extent paths
		temp = realloc(data, ((*len) + (ex->e_numreplicas *
										sizeof(sstack_file_handle_t))));
		if (NULL == temp) {
			sfs_log(ctx, SFS_ERR, "%s: Failed to allocate memory for "
				"storing extents of inode %lld\n",  __FUNCTION__,
				inode->i_num);
			free(data); // Freeup old
			return NULL;
		}
		data = temp;
		memcpy((void *) (data + (*len)), ex->e_path,
						(ex->e_numreplicas * sizeof(sstack_file_handle_t)));
		*len += (ex->e_numreplicas * sizeof(sstack_file_handle_t));
		ex ++;
	}

	sfs_log(ctx, SFS_DEBUG, "%s() - %d\n", __FUNCTION__,__LINE__);
	if (inode->i_numclients) {
		// 4. sfsd info
		temp = realloc(data, ((*len) + (sizeof(sstack_sfsd_info_t *) *
						(inode->i_numclients - 1))));
		if (NULL == temp) {
			sfs_log(ctx, SFS_ERR, "%s: Failed to allocate memory for "
					"storing sfsd info  of inode %lld\n",  __FUNCTION__,
					inode->i_num);
			free(data); // Freeup old
			return NULL;
		}
		data = temp;
		memcpy((void *) (data + (*len)), inode->i_sfsds,
				(sizeof(sstack_sfsd_info_t *) * (inode->i_numclients - 1)));
		*len += (sizeof(sstack_sfsd_info_t *) * inode->i_numclients);
	}
	sfs_log(ctx, SFS_DEBUG, "%s() - %d\n", __FUNCTION__,__LINE__);
	return data;
}

/*
 * put_inode -  Store flattened inode structure onto db
 *
 * inode - in-core inode structure . Should be non-NULL
 * db - DB pointer. Should be non-NULL
 * update - Is this an update? If so, send 1. Else 0
 *
 * Returns 0 on success and negative number indicating error on failure.
 */


int
put_inode(sstack_inode_t *inode, db_t *db, int update)
{
	char inode_str[MAX_INODE_LEN] = { '\0' };
	char *data = NULL;
	size_t len = 0;
	int ret = -1;
	unsigned long long inode_num = 0;

	// Parameter validation
	if (NULL == inode || NULL == db || update < 0 || update > 1) {
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
	sfs_log(db->ctx, SFS_DEBUG, "%s() - Before flatten inode\n",
			__FUNCTION__);
	data = flatten_inode(inode, &len, db->ctx);
	if (NULL == data) {
		sfs_log(db->ctx, SFS_ERR, "%s: Unable to flatten inode %lld\n",
				__FUNCTION__, inode_num);

		return -1;
	}

	if (update == 0) {
		if (db->db_ops.db_insert && ((db->db_ops.db_insert(inode_str, data,
							len, INODE_TYPE, db->ctx)) == 1)) {
			sfs_log(db->ctx, SFS_INFO, "%s: Succeeded for inode %lld \n",
				__FUNCTION__, inode_num);

			ret = 0;
		} else {
			sfs_log(db->ctx, SFS_ERR, "%s: Failed for inode %lld \n",
				__FUNCTION__, inode_num);

			ret = -2;
		}
	} else if (update == 0) {
		if (db->db_ops.db_insert && ((db->db_ops.db_update(inode_str, data,
							len, INODE_TYPE, db->ctx)) == 1)) {
			sfs_log(db->ctx, SFS_INFO, "%s: Succeeded for inode %lld \n",
				__FUNCTION__, inode_num);

			ret = 0;
		} else {
			sfs_log(db->ctx, SFS_ERR, "%s: Failed for inode %lld \n",
				__FUNCTION__, inode_num);

			ret = -2;
		}
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

inline void
sstack_free_erasure(log_ctx_t *ctx,
				sstack_extent_t *erasure,
				int num_erasure)
{
	sstack_extent_t *er;
	int i = 0;

	// Parameter validation
	if (NULL == erasure) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid parameter. "
					"Could indicate memory leak \n", __FUNCTION__);
		return;
	}

	er = erasure;

	while (i < num_erasure) {
		free(er->e_path);
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

/*
 * sstack_free_inode_res - Helper function to free up memory allocated
 *							to inode during get_inode
 *
 * inode - inode structure
 */

inline void
sstack_free_inode_res(sstack_inode_t *inode, log_ctx_t *ctx)
{
	if (NULL == inode)
		return;

	if (inode->i_xattrlen > 0) {
		free(inode->i_xattr);
	}
	if (inode->i_numextents > 0) {
		free(inode->i_extent);
	}

	if ((inode->i_numerasure > 0) && inode->i_erasure) {
		sstack_free_erasure(ctx, inode->i_erasure, inode->i_numerasure);
	}
}

/*
 * create_hash - Create SHA1 hash of the input
 *
 * input - Array of bytes to be hashed. Should be non-NULL
 * length - Length of the input array. Should be >0
 * result - SHA1 key. Should be non-NULL and at least 32 bytes long
 * ctx - log context
 *
 * Returns non-NULL SHA1 key upon success. Returns NULL upon failure.
 */
uint8_t *
create_hash(void *input, size_t length, uint8_t *result, log_ctx_t *ctx)
{
	SHA256_CTX context;

	// Parameter validation
	if (NULL == input || length <= 0 || NULL == result) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);
		errno = EINVAL;

		return NULL;
	}

	// Calculate SHA1 for the input
	if (SHA256_Init(&context) == 0) {
		sfs_log(ctx, SFS_ERR, "%s: SHA256_Init failed \n", __FUNCTION__);

		return NULL;
	}

	if (SHA256_Update(&context, (unsigned char *) input, length) == 0) {
		sfs_log(ctx, SFS_ERR, "%s: SHA256_Update failed \n", __FUNCTION__);

		return NULL;
	}

	if (SHA256_Final(result, &context) == 0) {
		sfs_log(ctx, SFS_ERR, "%s: SHA256_Final failed \n", __FUNCTION__);

		return NULL;
	}

	return result;
}

/* Trnasport helper functions */
inline sstack_transport_t *
alloc_transport(void)
{
	sstack_transport_t *transport;

	transport = malloc(sizeof(sstack_transport_t));

	return transport;
}

inline void
free_transport(sstack_transport_t *transport)
{
	if (transport)
		free(transport);
}

/* Generic functions */

char *
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

