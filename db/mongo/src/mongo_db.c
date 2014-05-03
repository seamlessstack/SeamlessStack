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

#include <stdio.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <mongo_db.h>
#include <sstack_log.h>

pthread_rwlock_t mongo_db_lock = PTHREAD_RWLOCK_INITIALIZER;
mongo conn[1];


// db_open, db_close are NOP for this DB

int
mongo_db_open(log_ctx_t * ctx)
{
	int ret = -1;

	// Initialize the connection
	ret = mongo_client(conn, MONGO_IP, MONGO_PORT);
	if (ret != MONGO_OK) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to connect to mongodb. "
			"Error = %d\n", __FUNCTION__, ret);
		return -1;
	}
	return 0;
}

int
mongo_db_close(log_ctx_t *ctx)
{
	return 0;
}

// db_init is where directory structure for the db is/are created
//

int
mongo_db_init(log_ctx_t *ctx)
{
	int ret = -1;
	char *mongo_ip = NULL;

	// Get Mongo DB IP address from environment
	mongo_ip = getenv("MONGO_IPADDR");
	if (NULL == mongo_ip) {
		// Initialize the connection
		ret = mongo_client(conn, MONGO_IP, MONGO_PORT);
		if (ret != MONGO_OK) {
			sfs_log(ctx, SFS_ERR, "%s: Failed to connect to mongodb. "
					"Error = %d\n", __FUNCTION__, ret);
			return -1;
		}
	} else {
		// Initialize the connection
		ret = mongo_client(conn, mongo_ip, MONGO_PORT);
		if (ret != MONGO_OK) {
			sfs_log(ctx, SFS_ERR, "%s: Failed to connect to mongodb. "
					"Error = %d\n", __FUNCTION__, ret);
			return -1;
		}
	}

#ifdef MONGO_TEST
	// For testing only. Will be done only in db_cleanup
	// Cleanup all collections
	ret = mongo_cmd_drop_collection(conn, DB_NAME, POLICY_COLLECTION, NULL);
	if (ret != MONGO_OK) {
		sfs_log(ctx, SFS_INFO,
			"%s: Collection %s does not exist in db %s\n",
			__FUNCTION__, POLICY_COLLECTION, DB_NAME);
	}

	ret = mongo_cmd_drop_collection(conn, DB_NAME, INODE_COLLECTION, NULL);
	if (ret != MONGO_OK) {
		sfs_log(ctx, SFS_INFO,
			"%s: Collection %s does not exist in db %s\n",
			__FUNCTION__, POLICY_COLLECTION, DB_NAME);
	}

	ret = mongo_cmd_drop_collection(conn, DB_NAME, JOURNAL_COLLECTION, NULL);
	if (ret != MONGO_OK) {
		sfs_log(ctx, SFS_INFO,
			"%s: Collection %s does not exist in db %s\n",
			__FUNCTION__, POLICY_COLLECTION, DB_NAME);
	}

	ret = mongo_cmd_drop_collection(conn, DB_NAME, CONFIG_COLLECTION, NULL);
	if (ret != MONGO_OK) {
		sfs_log(ctx, SFS_INFO,
			"%s: Collection %s does not exist in db %s\n",
			__FUNCTION__, POLICY_COLLECTION, DB_NAME);
	}
#endif // MONGO_TEST
#if 0
	// Go ahead and create collections
	// FIXME:
	// Uncapped collections are not working.
	// Try gridfs stuff with latest library
	ret = mongo_create_collection(conn, DB_NAME, POLICY_COLLECTION,
		10000,  NULL);
	if (ret != MONGO_OK) {
		sfs_log(ctx, SFS_ERR, "%s: Creating policy collection failed. "
			" Error = %d \n", __FUNCTION__, ret);
		return -1;
	}

	ret = mongo_create_collection(conn, DB_NAME, INODE_COLLECTION,
		10000, NULL);
	if (ret != MONGO_OK) {
		sfs_log(ctx, SFS_ERR, "%s: Creating inode collection failed. \n",
			__FUNCTION__);
		return -1;
	}

	ret = mongo_create_collection(conn, DB_NAME, JOURNAL_COLLECTION,
		10000, NULL);
	if (ret != MONGO_OK) {
		sfs_log(ctx, SFS_ERR, "%s: Creating journal collection failed.\n",
			__FUNCTION__);
		return -1;
	}

	ret = mongo_create_collection(conn, DB_NAME, CONFIG_COLLECTION,
		10000, NULL);
	if (ret != MONGO_OK) {
		sfs_log(ctx, SFS_ERR, "%s: Creating config collection failed.\n ",
			__FUNCTION__);
		return -1;
	}
#endif

	return 0;
}

// Meat of the DB
int
mongo_db_insert(char *key, char *data, size_t len, db_type_t type,
				log_ctx_t *ctx)
{
	bson b[1];
	int ret = -1;
	char db_and_collection[NAME_SIZE];
	char record_name[REC_NAME_SIZE];

	if (key == NULL) {
		sfs_log(ctx, SFS_ERR, "%s: Key passed is NULL. \n", __FUNCTION__);
		return -EINVAL;
	}

	bson_init(b);
	ret = bson_append_string(b, "record_num", key);
	if (ret != BSON_OK) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to append record_num to bson for "
			"inode %s. Error = %d \n", __FUNCTION__, key, ret);
		bson_destroy(b);
		return -ret;
	}

	ret = construct_record_name(record_name, type);
	if (ret != 0) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid record type %d specified\n",
			__FUNCTION__, type);
		bson_destroy(b);
		return -1;
	}

	ret = bson_append_binary(b, record_name, BSON_BIN_BINARY,
		(const char *) data, len);
	if (ret != BSON_OK) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to append inode to bson for"
			" inode %s. Error = %d \n", __FUNCTION__, key, ret);
		bson_destroy(b);
		return -ret;
	}

	ret = bson_append_long(b, "record_length", len);
	if (ret != BSON_OK) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to append size to bson for "
			"inode %s. Error = %d \n", __FUNCTION__, key, ret);
		bson_destroy(b);
		return -ret;
	}

	bson_finish(b);
	ret = construct_db_name(db_and_collection, type);
	if (ret != 0) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid type specified. db_type = %d\n",
			__FUNCTION__, type);
		bson_destroy(b);
		return -1;
	}

	pthread_rwlock_wrlock(&mongo_db_lock);
	ret = mongo_insert(conn, db_and_collection, b, NULL);
	if (ret != MONGO_OK) {
		sfs_log(ctx, SFS_ERR,
			"%s: Failed to insert record %s of type %d"
			"into db.collection %s. Error = %d\n",
			__FUNCTION__, key, (int) type, db_and_collection, ret);
		bson_destroy(b);
		return -ret;
	}
	sfs_log(ctx, SFS_INFO,
		"%s: Successfully inserted record %s of type %d"
		" into db.collection %s\n",
		__FUNCTION__, key, (int) type, db_and_collection);
	pthread_rwlock_unlock(&mongo_db_lock);
	bson_destroy(b);

	return 1;
}


int
mongo_db_remove(char *key, db_type_t type, log_ctx_t *ctx)
{
	bson b[1];
	int ret = -1;
	char db_and_collection[NAME_SIZE];
	char record_name[REC_NAME_SIZE];

	if (key == NULL) {
		sfs_log(ctx, SFS_ERR, "%s: Key passed is NULL. \n", __FUNCTION__);
		return -EINVAL;
	}
	bson_init(b);
	ret = bson_append_string(b, "record_num", key);
	if (ret != BSON_OK) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to append record_num to bson for "
			"inode %s. Error = %d \n", __FUNCTION__, key, ret);
		bson_destroy(b);
		return -ret;
	}
	bson_finish(b);
	ret = construct_db_name(db_and_collection, type);
	if (ret != 0) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid type specified. db_type = %d\n",
			__FUNCTION__, type);
		bson_destroy(b);
		return -1;
	}

	pthread_rwlock_wrlock(&mongo_db_lock);
	ret = mongo_remove(conn, db_and_collection, b, NULL);
	if (ret != MONGO_OK) {
		sfs_log(ctx, SFS_ERR,
			"%s: Failed to remove record %s of type %d"
			"into db.collection %s. Error = %d\n",
			__FUNCTION__, key, (int) type, db_and_collection, ret);
		bson_destroy(b);
		return -ret;
	}
	sfs_log(ctx, SFS_INFO,
		"%s: Successfully removed record %s of type %d"
		" into db.collection %s\n",
		__FUNCTION__, key, (int) type, db_and_collection);
	pthread_rwlock_unlock(&mongo_db_lock);
	bson_destroy(b);

	return 1;
}


// Seek to 'offset' from 'whence' and read data
// Uses gzseek and gzread
// Needed to read extents from inode
int
mongo_db_seekread(char * key, char *data, size_t len, off_t offset,
	int whence, db_type_t type, log_ctx_t *ctx)
{
	int ret = -1;
	bson query[1];
	bson response[1];
	bson_iterator iter[1];
	char db_and_collection[NAME_SIZE];
	char record_name[REC_NAME_SIZE];
	uint64_t record_len = 0;
	char *record;

	if (key == NULL) {
		sfs_log(ctx, SFS_ERR, "%s: Key passed is NULL. \n", __FUNCTION__);
		return -EINVAL;
	}

	if (NULL == data) {
		sfs_log(ctx, SFS_ERR, "%s: data pointer is NULL. \n",
			__FUNCTION__);
		return -EINVAL;
	}

	bson_init(query);
	bson_append_string(query, "record_num", key);
	if (ret != BSON_OK) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to append record_num to bson for "
			"inode %s. Error = %d \n", __FUNCTION__, key, ret);
		bson_destroy(query);
		return -ret;
	}
	bson_finish(query);

	// We expect only one document per collection for a given key
	// So mongo_find_one suffices
	ret = construct_db_name(db_and_collection, type);
	if (ret != 0) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid type specified. db_type = %d\n",
			__FUNCTION__, type);
		bson_destroy(query);
		return -1;
	}
	ret = mongo_find_one(conn, db_and_collection, query, bson_shared_empty(),
		response);

	if (ret != MONGO_OK) {
		sfs_log(ctx, SFS_ERR,
			"%s: Failed to retrieve record for %s from"
			" collection %s. Error = %d\n",
			__FUNCTION__, key, db_and_collection, ret);
		bson_destroy(query);
		return -ret;
	}

	ret = construct_record_name(record_name, type);
	if (ret != 0) {
		// This is just to catch stack corruption if any
		sfs_log(ctx, SFS_ERR, "%s: Invalid type specified. db_type = %d\n",
			__FUNCTION__, type);
		bson_destroy(query);
		return -1;
	}

	(void)bson_find(iter, response, "record_length");
	record_len = bson_iterator_long(iter);
	// Allocate memory for data
	record = malloc(record_len);
	if (NULL == record) {
		sfs_log(ctx, SFS_ERR,
			"%s: Failed to allocate memory for reading record."
			"Requested size = %"PRId64" key = %s db name = %s\n",
			__FUNCTION__, record_len, key, db_and_collection);
		bson_destroy(query);
		return -ENOMEM;
	}
	(void)bson_find(iter, response, record_name);
	memcpy(record, bson_iterator_bin_data(iter), record_len);
	// Offset it by offset
	// Assuming whence to be SEEK_SET
	memcpy(data, record + offset, len);
	bson_destroy(query);
	bson_destroy(response);

	return len;
}


void
mongo_db_iterate(db_type_t type, iterator_function_t iterator_fn, void *params,
				log_ctx_t *ctx)
{
	int ret = -1;
	bson *response;
	bson_iterator iter[1];
	mongo_cursor cursor[1];
	char db_and_collection[NAME_SIZE];
	char *key;
	void *data;
	uint64_t record_len;
	char *record_name;

	if (iterator_fn == NULL) {
		sfs_log(ctx, SFS_ERR, "%s: No iterator function supplied\n",
				__FUNCTION__);
		return;
	}

	ret = construct_db_name(db_and_collection, type);
	if (ret != 0) {
		sfs_log(ctx, SFS_ERR,
				"%s: Invalid type specified. db_type = %d\n",
				__FUNCTION__, type);
		return;
	}
	mongo_cursor_init(cursor, conn, db_and_collection);

	while (mongo_cursor_next(cursor) == MONGO_OK) {
		response = &cursor->current;
		bson_find(iter, response, "record_num");
		key = (char *)bson_iterator_string(iter);
		construct_record_name(record_name, type);
		bson_find(iter, response, record_name);
		data = (void *)bson_iterator_bin_data(iter);
		bson_find(iter, response, "record length");
		record_len = bson_iterator_long(iter);
		iterator_fn(params, key, data, record_len);
	}
	return;
}

/*
 * mongo_db_get - Get one record specified by key from MongoDB collection
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
mongo_db_get(char *key, char **data, db_type_t type, log_ctx_t *ctx)
{
	int ret = -1;
	bson query[1];
	bson response[1];
	bson_iterator iter[1];
	char db_and_collection[NAME_SIZE];
	char record_name[REC_NAME_SIZE];
	uint64_t record_len = 0;


	sfs_log(ctx, SFS_DEBUG, "%s: key = %s type = %d \n",
			__FUNCTION__, key, type);
	if (key == NULL) {
		sfs_log(ctx, SFS_ERR, "%s: Key passed is NULL. \n",
				__FUNCTION__);
		return -EINVAL;
	}

	bson_init(query);
	bson_append_string(query, "record_num", key);
	bson_finish(query);

	// We expect only one document per collection for a given key
	// So mongo_find_one suffices
	ret = construct_db_name(db_and_collection, type);
	if (ret != 0) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid type specified. db_type = %d\n",
			__FUNCTION__, type);
		bson_destroy(query);
		return -1;
	}
	pthread_rwlock_rdlock(&mongo_db_lock);
	ret = mongo_find_one(conn, db_and_collection, query,
		bson_shared_empty(), response);

	if (ret != MONGO_OK) {
		sfs_log(ctx, SFS_ERR,
			"%s: Failed to retrieve record for %s from"
			" collection %s. Error = %d\n",
			__FUNCTION__, key, db_and_collection, ret);
		bson_destroy(query);
		pthread_rwlock_unlock(&mongo_db_lock);
		return -ret;
	}
	pthread_rwlock_unlock(&mongo_db_lock);

	ret = construct_record_name(record_name, type);
	if (ret != 0) {
		// This is just to catch stack corruption if any
		sfs_log(ctx, SFS_ERR,
				"%s: Invalid type specified. db_type = %d\n",
			__FUNCTION__, type);
		bson_destroy(query);
		return -1;
	}

	(void)bson_find(iter, response, "record_length");
	record_len = bson_iterator_long(iter);
	(void)bson_find(iter, response, record_name);
	// Allocate buffer
	sfs_log(ctx, SFS_DEBUG, "%s: Record length for key %s is %d\n",
		   __FUNCTION__, key, record_len);
	*data = malloc(record_len);
	if (NULL == *data) {
		sfs_log(ctx, SFS_ERR, "%s: Unable to allocate %d bytes for "
				"reading inode record for inode %s. \n",
				__FUNCTION__, record_len, key);

		return -EINVAL;
	}
	memcpy(*data, bson_iterator_bin_data(iter), record_len);

	bson_destroy(query);
	bson_destroy(response);

	sfs_log(ctx, SFS_INFO, "%s: Retruning %d\n", __FUNCTION__, record_len);

	return record_len;
}

int
mongo_db_update(char *key, char *data, size_t len, db_type_t type,
		log_ctx_t *ctx)
{
	int ret = -1;
	bson b[1];
	char db_and_collection[NAME_SIZE];
	char record_name[REC_NAME_SIZE];

	bson_init(b);
	ret = bson_append_string(b, "record_num", key);
	if (ret != MONGO_OK) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to append record_num to bson for "
			"inode %s. Error = %d \n", __FUNCTION__, key, ret);
		bson_destroy(b);
		return -ret;
	}

	ret = construct_record_name(record_name, type);
	if (ret != 0) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid record type %d specified\n",
			__FUNCTION__, type);
		bson_destroy(b);
		return -1;
	}

	ret = bson_append_binary(b, record_name, BSON_BIN_BINARY,
		(const char *) data, len);
	if (ret != BSON_OK) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to append inode to bson for"
			" inode %s. Error = %d \n", __FUNCTION__, key, ret);
		bson_destroy(b);
		return -ret;
	}

	ret = bson_append_long(b, "record_length", len);
	if (ret != BSON_OK) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to append size to bson for "
			"inode %s. Error = %d \n", __FUNCTION__, key, ret);
		bson_destroy(b);
		return -ret;
	}

	bson_finish(b);

	ret = construct_db_name(db_and_collection, type);
	if (ret != 0) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid type specified. db_type = %d\n",
			__FUNCTION__, type);
		bson_destroy(b);
		return -1;
	}

	pthread_rwlock_wrlock(&mongo_db_lock);
	ret = mongo_update(conn, db_and_collection, bson_shared_empty(), b,
			0, NULL);
	if (ret != MONGO_OK) {
		sfs_log(ctx, SFS_ERR, "%s: Record updation for key %s into "
			"database %s failed with error %d\n",
			__FUNCTION__, key, db_and_collection, ret);
		pthread_rwlock_unlock(&mongo_db_lock);
		bson_destroy(b);
		return -ret;
	}
	pthread_rwlock_unlock(&mongo_db_lock);
	bson_destroy(b);

	return len;
}

int
mongo_db_delete(char *key, log_ctx_t *ctx)
{
	int ret = -1;

	// For testing only. Will be done only in db_cleanup
	// Cleanup all collections
	ret = mongo_cmd_drop_collection(conn, DB_NAME, POLICY_COLLECTION, NULL);
	if (ret != MONGO_OK) {
		sfs_log(ctx, SFS_INFO,
			"%s: Collection %s does not exist in db %s\n",
			__FUNCTION__, POLICY_COLLECTION, DB_NAME);
		ret = -1;
		goto out;
	}
	ret = mongo_cmd_drop_collection(conn, DB_NAME, INODE_COLLECTION, NULL);
	if (ret != MONGO_OK) {
		sfs_log(ctx, SFS_INFO,
			"%s: Collection %s does not exist in db %s\n",
			__FUNCTION__, POLICY_COLLECTION, DB_NAME);
		ret = -1;
		goto out;
	}
	ret = mongo_cmd_drop_collection(conn, DB_NAME, JOURNAL_COLLECTION, NULL);
	if (ret != MONGO_OK) {
		sfs_log(ctx, SFS_INFO,
			"%s: Collection %s does not exist in db %s\n",
			__FUNCTION__, POLICY_COLLECTION, DB_NAME);
		ret = -1;
		goto out;
	}
	ret = mongo_cmd_drop_collection(conn, DB_NAME, CONFIG_COLLECTION, NULL);
	if (ret != MONGO_OK) {
		sfs_log(ctx, SFS_INFO,
			"%s: Collection %s does not exist in db %s\n",
			__FUNCTION__, POLICY_COLLECTION, DB_NAME);
		ret = -1;
	}

out:
	return ret;
}

int
mongo_db_cleanup(log_ctx_t *ctx)
{
	mongo_db_delete(NULL, ctx);
	mongo_destroy(conn);
	sfs_log(ctx, SFS_INFO, "%s: DB %s destroyed \n", __FUNCTION__, DB_NAME);

	return 0;
}
