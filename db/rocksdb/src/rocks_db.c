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
#include <rocksdb/c.h>
#include <sstack_db.h>
#include <sstack_log.h>
#include <sstack_rocks_db.h>

static rocksdb_t *rocksdb = NULL;
static rocksdb_writeoptions_t *write_options = NULL;
static rocksdb_readoptions_t *read_options = NULL;
static rocksdb_options_t *options = NULL;

static int
cmp_compare(void *arg, const char *s1, size_t s1_len, const char *s2,
		size_t s2_len)
{
	int min_len = (s1_len < s2_len) ? s1_len : s2_len;
	int ret = memcmp(s1, s2, min_len);
	if (ret == 0) {
		if (s1_len < s2_len)
			ret = -1;
		else if (s1_len > s2_len)
			ret = 1;
	}

	return ret;
}

static void
cmp_destroy(void *arg)
{
	// Place holder. Needed for prototype
}

static const char *
cmp_name(void *arg)
{
	return "rocksdb_cmp";
}


int
rocks_db_init(log_ctx_t *ctx)
{
	rocksdb_filterpolicy_t *filter;
	rocksdb_cache_t *cache;
	rocksdb_comparator_t *cmp;
	rocksdb_env_t *env;
	int compression_levels[] = { rocksdb_no_compression,
								rocksdb_no_compression,
								rocksdb_no_compression,
								rocksdb_no_compression};

	options = rocksdb_options_create();
	// Set conmparator
	cmp = rocksdb_comparator_create(NULL, cmp_destroy, cmp_compare, cmp_name);
	rocksdb_options_set_comparator(options, cmp);
	// Create a new one if missing
	rocksdb_options_set_create_if_missing(options, T);
	// Check for corruptions . Be parannoid ...
	rocksdb_options_set_paranoid_checks(options, T);
	// Info logger. Not needed
	rocksdb_options_set_info_log(options, NULL);
	// Set default environment
	env = rocksdb_create_default_env();
	rocksdb_options_set_env(options, env);
	// In-memory table size
	rocksdb_options_set_write_buffer_size(options, WRITE_BUFFER_SIZE);
	// Number of open files used by rocksdb
	rocksdb_options_set_max_open_files(options, MAXIMUM_OPEN_FILES);
	// Maximum number of write buffers in memory. Used as ping-pong
	// buffers (i.e. when one buffer is full and is getting flushed, all
	// writes reach another buffer)
	rocksdb_options_set_max_write_buffer_number(options, MAX_WRITE_BUFFER_NUM);
	// Target file size for compaction
	// Higher the value lesser the number of compactions(write amp)
	rocksdb_options_set_target_file_size_base(options, TARGET_FILE_SIZE_BASE);
	// Bllom filter setting to use 10 bits for smaller false positive ratio
	filter = rocksdb_filterpolicy_create_bloom(NUM_BLLOM_FILTER_BITS);
	rocksdb_options_set_filter_policy(options, filter);
	// LRU cache size
	cache = rocksdb_cache_create_lru(LRU_CACHE_SIZE);
	rocksdb_options_set_cache(options, cache);
	// Maximum number of threads used for compactions
	rocksdb_options_set_max_background_compactions(options,
			MAX_COMPACTION_THREADS);
	// Maximum number of threads used for memtable flushes
	rocksdb_options_set_max_background_flushes(options,
			MAX_BACKGROUND_FLUSH_THREADS);
	// Compression policy
	// By default, RocksDB uses snappy compression
	// Though snappy is a very fast compression algorithm it may still
	// slow us down. Setting compression to nothing for now.
	// TODO
	// Experiment how much savings we get with snappy and may enable it
	// by commenting following lines
	// These are set in sample program at rocksdb/db/c_test.c
	rocksdb_options_set_compression(options, rocksdb_no_compression);
	rocksdb_options_set_compression_options(options, -14, -1, 0);
	rocksdb_options_set_compression_per_level(options, compression_levels, 4);

	rocksdb = rocksdb_open(options, TRANSACTIONDB_NAME, NULL);

	if (rocksdb) {
		write_options = rocksdb_writeoptions_create();
		// This is required for consistency though causes writes to be slower
		rocksdb_write_options_set_sync(write_options, 1);
		read_options = rocksdb_readoptions_create();
		// Data integrity check
		rocksdb_readoptions_set_verify_checksum(read_options, 1);
		rocksdb_readoptions_set_fill_cache(read_options, 0);

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
	char *err = NULL;

	// Parameter validation
	if (NULL == key) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid parameter specified \n",
				__FUNCTION__);

		errno = EINVAL;
		return -1;
	}

	rocksdb_put(rocksdb, write_options, key, strlen(key), value,
			value_len, &err);
	if (err != NULL) {
		sfs_log(ctx, SFS_ERR, "%s: rocksdb_put failed for key %s . "
				"Error = %s\n", __FUNCTION__, key, err);

		return -1;
	}

	return 0;
}


int
rocks_db_remove(char *key, db_type_t type, log_ctx_t *ctx)
{
	char *err = NULL;

	// Parameter validation
	if (NULL == key) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid parameter specified \n",
				__FUNCTION__);

		errno = EINVAL;
		return -1;
	}

	rocksdb_delete(rocksdb, write_options, key, strlen(key), &err);
	if (err != NULL) {
		sfs_log(ctx, SFS_ERR, "%s: rocksdb_delete failed for key %s . "
				"Error = %s\n", __FUNCTION__, key, err);

		return -1;
	}

	return 0;
}

int
rocks_db_seekread(char *key, char *data, size_t len, off_t offset,
		int whence, db_type_t type, log_ctx_t *ctx)
{
	sfs_log(ctx, SFS_DEBUG, "%s: dummy function \n", __FUNCTION__);

	return 0;
}

/*
 * rocks_db_get - Get one record specified by key from RocksDB
 *  represented by type
 *
 * key - Key for the record
 * data - O/P parameter that holds read record.
 * type - Type of the collection
 *
 * Returns number of bytes read  on success and negative number
 * indicating error on failure.
 */

int
rocks_db_get(char *key, char **data, db_type_t type, log_ctx_t *ctx)
{
	size_t value_len = 0;
	char *err = NULL;

	// Parameter validation
	if (NULL == key) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid parameter specified \n",
				__FUNCTION__);

		errno = EINVAL;
		return -1;
	}

	*data = rocksdb_get(rocksdb, read_options, key, strlen(key),
				&value_len, &err);
	if (err != NULL) {
		sfs_log(ctx, SFS_ERR, "%s: rocksdb_get failed for key %s . "
				"Error = %s\n", __FUNCTION__, key, err);

		return -1;
	}
	sfs_log(ctx, SFS_DEBUG, "%s: key = %s value = %s\n",
			__FUNCTION__, key, *data);

	return value_len;
}

/*
 * rocks_db_update - Update one record specified by key from RocksDB
 *  represented by type
 *
 * key - Key for the record
 * data - Data to be updated
 * len - Length of the value
 * type - Type of the collection
 *
 * Returns 0 on success and -1 upon failure
 */
int
rocks_db_update(char *key, char *data, size_t len, db_type_t type,
		log_ctx_t *ctx)
{
	char *err = NULL;
	// Parameter validation
	if (NULL == key) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid parameter specified \n",
				__FUNCTION__);

		errno = EINVAL;
		return -1;
	}

	// Update would cause read-modify-write
	// rocksdb_merge is supposed to avoid read-modify-write
	rocksdb_merge(rocksdb, write_options, key, strlen(key), data, len, &err);
	if (err != NULL) {
		sfs_log(ctx, SFS_ERR, "%s: rocksdb_merge failed for key %s . "
				"Error = %s\n", __FUNCTION__, key, err);

		return -1;
	}

	return 0;
}

int
rocks_db_delete(char *key, log_ctx_t *ctx)
{
	char *err = NULL;

	rocksdb_delete(rocksdb, write_options, key, strlen(key), &err);
	if (err != NULL) {
		sfs_log(ctx, SFS_ERR, "%s: rocksdb_delete failed for key %s . "
				"Error = %s\n", __FUNCTION__, key, err);

		return -1;
	}


	return 0;
}

int
rocks_db_cleanup(log_ctx_t *ctx)
{
	char *err = NULL;

	rocksdb_destroy_db(options, TRANSACTIONDB_NAME, &err);
	if (err != NULL) {
		sfs_log(ctx, SFS_ERR, "%s: rocksdb_destroy_db failed with error %s\n",
				 __FUNCTION__, err);

		return -1;
	}

	sfs_log(ctx, SFS_DEBUG, "%s: DB %s destroyed \n", __FUNCTION__,
			TRANSACTIONDB_NAME);

	return 0;
}


void
rocks_db_iterate(db_type_t type, iterator_function_t iterator_fn, void *params,
		log_ctx_t *ctx)
{
	rocksdb_iterator_t *iter = rocksdb_create_iterator(rocksdb, read_options);
	const char *key = NULL;
	const char *data = NULL;

	if (NULL == iterator_fn) {
		sfs_log(ctx, SFS_ERR, "%s: No iterator function supplied\n",
				__FUNCTION__);

		return;
	}

	// Set to first record
	rocksdb_iter_seek_to_first(iter);

	while(rocksdb_iter_valid(iter)) {
		size_t len;

		// Get key
		key = rocksdb_iter_key(iter, &len);
		if (NULL == key) {
			sfs_log(ctx, SFS_ERR, "%s: key returned is NULL \n", __FUNCTION__);

			return;
		}

		// get value
		data = rocksdb_iter_value(iter, &len);
		if (NULL == data) {
			sfs_log(ctx, SFS_ERR, "%s: data returned is NULL \n", __FUNCTION__);

			return;
		}

		iterator_fn(params, (char *) key, (char *) data, len);

		// Point to next record
		rocksdb_iter_next(iter);
	}

	return;
}
