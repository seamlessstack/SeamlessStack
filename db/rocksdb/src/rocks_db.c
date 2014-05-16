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
#include <c.h> // RocksDB C bindings
#include <sstack_db.h>
#include <sstack_log.h>

static rocksdb_t *rocksdb = NULL;
static rocksdb_writeoptions_t *write_options = NULL;
static rocksdb_readoptions_t *write_options = NULL;

int
rocks_db_init(log_ctx_t *ctx)
{
	rocksdb_options_t options;

	rocksdb = rocksdb_open(&options, TRANSACTIONDB_NAME, NULL);

	if (rocksdb) {
		write_options = rocksdb_writeoptions_create();
		// This is required for consistency though causes writes to be slower
		rocksdb_write_options_set_sync(write_options, 1);
		read_options = rocksdb_readoptions_create();

		sfs_log(ctx, SFS_DEBUG, "%s: succeeded. rocksdb = %"PRIx64"\n",
				__FUNCTION__, rocksdb);
		return 0;
	} else {
		sfs_log(ctx, SFS_ERR, "%s: failed. \n", __FUNCTION__);
		return -1;
	}

}

int
rocks_db_open(log_ctx_t *ctx)
{
	sfs_log(ctx, SFS_DEBUG, "%s: dummy function \n", __FUNCTION__);

	return 0;
}


int
rocks_db_close(log_ctx_t *ctx)
{
	sfs_log(ctx, SFS_DEBUG, "%s: dummy function \n", __FUNCTION__);

	return 0;
}

int
rocks_db_insert(char *key, char *value, size_t value_len, db_type_t type,
		log_ctx_t *ctx)
{

	// Parameter validation
	if (NULL == key) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid parameter specified \n",
				__FUNCTION__);

		errno = EINVAL;
		return -1;
	}

	rocksdb_put(rocksdb, write_options, key, strlen(key), value,
			value_len, NULL);

	return 0;
}


int
rocks_db_remove(char *key, db_type_t type, log_ctx_t *ctx)
{
	// Parameter validation
	if (NULL == key) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid parameter specified \n",
				__FUNCTION__);

		errno = EINVAL;
		return -1;
	}

	rocksdb_delete(rocksdb, write_options, key, strlen(key), NULL);

	return 0;
}

int
rocks_db_seekread(char *key, char *data, size_t len, off_t offset,
		int whence, db_type_t type, log_ctx_t *ctx)
{
	sfs_log(ctx, SFS_DEBUG, "%s: dummy function \n", __FUNCTION__);

	return 0;
}

int
rocks_db_get(char *key, char **data, db_type_t type, log_ctx_t *ctx)
{
	size_t value_len = 0;

	// Parameter validation
	if (NULL == key) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid parameter specified \n",
				__FUNCTION__);

		errno = EINVAL;
		return NULL;
	}

	*data = rocksdb_get(rocksdb, read_options, key, strlen(key),
				&value_len, NULL);

	return *data;
}

int
rocks_db_update(char *key, char *data, size_t len, db_type_t type,
		log_ctx_t *ctx)
{
	// Parameter validation
	if (NULL == key) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid parameter specified \n",
				__FUNCTION__);

		errno = EINVAL;
		return NULL;
	}

	// Update would cause read-modify-write
	// rocksdb_merge is supposed to avoid read-modify-write
	rocksdb_merge(rocksdb, write_options, key, strlen(key), data, len, NULL);

	return 0;
}

int
rocks_db_delete(char *key, log_ctx_t *ctx)
{
	rocksdb_options_t *options = rocksdb_options_create();

	rocksdb_destroy_db(options, TRANSACTIONDB_NAME, NULL);

	return 0;
}

int
rocks_db_cleanup(log_ctx_t *ctx)
{
	rocks_db_delete(NULL, ctx);

	sfs_log(ctx, SFS_DEBUG, "%s: DB %s destroyed \n", __FUNCTION__,
			TRANSACTIONDB_NAME);

	return 0;
}

void
rocks_db_iterate(db_type_t type, iterator_function_t iterator_fn, void *params,
		log_ctx_t *ctx)
{
	rocksdb_iterator_t *iter = rocksdb_create_iterator(rocksdb, read_options);

	// TODO

	return 0;
}

