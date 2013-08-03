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

#include <stdio.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <mongo_db.h>
#include <sstack_log.h>

pthread_mutex_t mongo_db_mutex = PTHREAD_MUTEX_INITIALIZER;
mongo conn[1];
extern log_ctx_t *sfs_ctx;


// db_open, db_close are NOP for this DB

int
mongo_db_open(void)
{
	return 0;
}

int
mongo_db_close(void)
{
	return 0;
}

// db_init is where directory structure for the db is/are created
// 

int
mongo_db_init(void)
{
	int ret = -1;

	// Initialize the connection
	ret = mongo_client(conn, MONGO_IP, MONGO_PORT);
	if (ret != MONGO_OK) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to connect to mongodb. "
			"Error = %d\n", __FUNCTION__, ret);
		return -1;
	}

#ifdef MONGO_TEST
	// For testing only. Will be done only in db_cleanup
	// Cleanup all collections
	ret = mongo_cmd_drop_collection(conn, DB_NAME, POLICY_COLLECTION, NULL);
	if (ret != MONGO_OK) {
		sfs_log(sfs_ctx, SFS_INFO,
			"%s: Collection %s does not exist in db %s\n",
			__FUNCTION__, POLICY_COLLECTION, DB_NAME);
	}

	ret = mongo_cmd_drop_collection(conn, DB_NAME, INODE_COLLECTION, NULL);
	if (ret != MONGO_OK) {
		sfs_log(sfs_ctx, SFS_INFO,
			"%s: Collection %s does not exist in db %s\n",
			__FUNCTION__, POLICY_COLLECTION, DB_NAME);
	}

	ret = mongo_cmd_drop_collection(conn, DB_NAME, JOURNAL_COLLECTION, NULL);
	if (ret != MONGO_OK) {
		sfs_log(sfs_ctx, SFS_INFO,
			"%s: Collection %s does not exist in db %s\n",
			__FUNCTION__, POLICY_COLLECTION, DB_NAME);
	}

	ret = mongo_cmd_drop_collection(conn, DB_NAME, CONFIG_COLLECTION, NULL);
	if (ret != MONGO_OK) {
		sfs_log(sfs_ctx, SFS_INFO,
			"%s: Collection %s does not exist in db %s\n",
			__FUNCTION__, POLICY_COLLECTION, DB_NAME);
	}
#endif // MONGO_TEST

	// Go ahead and create collections
	ret = mongo_create_collection(conn, DB_NAME, POLICY_COLLECTION,
		0, NULL);
	if (ret != MONGO_OK) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Creating policy collection failed. "
			"Default collections are capped which makes sharding impossible.\n",
			__FUNCTION__);
		return -1;
	}

	ret = mongo_create_collection(conn, DB_NAME, INODE_COLLECTION,
		0, NULL);
	if (ret != MONGO_OK) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Creating inode collection failed. "
			"Default collections are capped which makes sharding impossible.\n",
			__FUNCTION__);
		return -1;
	}

	ret = mongo_create_collection(conn, DB_NAME, JOURNAL_COLLECTION,
		0, NULL);
	if (ret != MONGO_OK) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Creating journal collection failed. "
			"Default collections are capped which makes sharding impossible.\n",
			__FUNCTION__);
		return -1;
	}

	ret = mongo_create_collection(conn, DB_NAME, CONFIG_COLLECTION,
		0, NULL);
	if (ret != MONGO_OK) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Creating config collection failed. "
			"Default collections are capped which makes sharding impossible.\n",
			__FUNCTION__);
		return -1;
	}

	return 0;
}

// Meat of the DB
int
mongo_db_insert(uint64_t key, char *data, size_t len, db_type_t type)
{
	bson b[1];
	int ret = -1;
	char db_and_collection[NAME_SIZE];
	char record_name[REC_NAME_SIZE];

	bson_init(b);
	ret = bson_append_long(b, "record_num", key);
	if (ret != BSON_OK) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to append record_num to bson for "
			"inode %"PRId64". Error = %d \n", __FUNCTION__, key, ret);
		bson_destroy(b);
		return -ret;
	}

	ret = construct_record_name(record_name, type);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid record type %d specified\n",
			__FUNCTION__, type);
		bson_destroy(b);
		return -1;
	}

	ret = bson_append_binary(b, record_name, BSON_BIN_BINARY,
		(const char *) data, len);
	if (ret != BSON_OK) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to append inode to bson for" 
			" inode %"PRId64". Error = %d \n", __FUNCTION__, key, ret);
		bson_destroy(b);
		return -ret;
	}

	ret = bson_append_long(b, "record_length", len);
	if (ret != BSON_OK) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to append size to bson for "
			"inode %"PRId64". Error = %d \n", __FUNCTION__, key, ret);
		bson_destroy(b);
		return -ret;
	}

	bson_finish(b);
	ret = construct_db_name(db_and_collection, type);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid type specified. db_type = %d\n",
			__FUNCTION__, type);
		bson_destroy(b);
		return -1;
	}

	pthread_mutex_lock(&mongo_db_mutex);
	ret = mongo_insert(conn, db_and_collection, b, NULL);
	if (ret != MONGO_OK) {
		sfs_log(sfs_ctx, SFS_ERR,
			"%s: Failed to insert record %"PRId64" of type %d"
			"into db.collection %s. Error = %d\n",
			__FUNCTION__, key, (int) type, db_and_collection, ret);
		bson_destroy(b);
		return -ret;
	}
	sfs_log(sfs_ctx, SFS_INFO,
		"%s: Successfully inserted record %"PRId64" of type %d"
		" into db.collection %s\n",
		__FUNCTION__, key, (int) type, db_and_collection);
	pthread_mutex_unlock(&mongo_db_mutex);
	bson_destroy(b);

	return 1;
}

// Seek to 'offset' from 'whence' and read data
// Uses gzseek and gzread
// Needed to read extents from inode
int
mongo_db_seekread(uint64_t key, char *data, size_t len, off_t offset,
	int whence, db_type_t type) 
{
	int ret = -1;
	bson query[1];
	bson response[1];
	bson_iterator iter[1];
	char db_and_collection[NAME_SIZE];
	char record_name[REC_NAME_SIZE];
	uint64_t record_len = 0;
	char *record;

	if (NULL == data) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: data pointer is NULL. \n",
			__FUNCTION__);
		return -EINVAL;
	}

	bson_init(query);
	bson_append_long(query, "record_num", key);
	bson_finish(query);

	// We expect only one document per collection for a given key
	// So mongo_find_one suffices
	ret = construct_db_name(db_and_collection, type);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid type specified. db_type = %d\n",
			__FUNCTION__, type);
		bson_destroy(query);
		return -1;
	}
	ret = mongo_find_one(conn, db_and_collection, query, bson_shared_empty(),
		response);

	if (ret != MONGO_OK) {
		sfs_log(sfs_ctx, SFS_ERR,
			"%s: Failed to retrieve record for %"PRId64" from"
			" collection %s. Error = %d\n",
			__FUNCTION__, key, db_and_collection, ret);
		bson_destroy(query);
		return -ret;
	}

	ret = construct_record_name(record_name, type);
	if (ret != 0) {
		// This is just to catch stack corruption if any
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid type specified. db_type = %d\n",
			__FUNCTION__, type);
		bson_destroy(query);
		return -1;
	}

	(void)bson_find(iter, response, "record_length");
	record_len = bson_iterator_long(iter);
	// Allocate memory for data
	record = malloc(record_len);
	if (NULL == record) {
		sfs_log(sfs_ctx, SFS_ERR,
			"%s: Failed to allocate memory for reading record."
			"Requested size = %"PRId64" key = %"PRId64" db name = %s\n",
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


int
mongo_db_get(uint64_t key, char *data, size_t len, db_type_t type)
{
	int ret = -1;
	bson query[1];
	bson response[1];
	bson_iterator iter[1];
	char db_and_collection[NAME_SIZE];
	char record_name[REC_NAME_SIZE];
	uint64_t record_len = 0;

	if (NULL == data || len <= 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified. \n",
			__FUNCTION__);
		return -EINVAL;
	}

	bson_init(query);
	bson_append_long(query, "record_num", key);
	bson_finish(query);

	// We expect only one document per collection for a given key
	// So mongo_find_one suffices
	ret = construct_db_name(db_and_collection, type);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid type specified. db_type = %d\n",
			__FUNCTION__, type);
		bson_destroy(query);
		return -1;
	}
	ret = mongo_find_one(conn, db_and_collection, query,
		bson_shared_empty(), response);

	if (ret != MONGO_OK) {
		sfs_log(sfs_ctx, SFS_ERR,
			"%s: Failed to retrieve record for %"PRId64" from"
			" collection %s. Error = %d\n",
			__FUNCTION__, key, db_and_collection, ret);
		bson_destroy(query);
		return -ret;
	}

	ret = construct_record_name(record_name, type);
	if (ret != 0) {
		// This is just to catch stack corruption if any
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid type specified. db_type = %d\n",
			__FUNCTION__, type);
		bson_destroy(query);
		return -1;
	}

	(void)bson_find(iter, response, "record_length");
	record_len = bson_iterator_long(iter);
	(void)bson_find(iter, response, record_name);
	// Check if requested length is bigger than the actual length
	if (record_len < len) {
		sfs_log(sfs_ctx, SFS_INFO,
			"%s: Record size is smaller than lenth requested."
			" Record len = %"PRId64" len = %d\n",
			__FUNCTION__, record_len, (int)len);
		memcpy(data, bson_iterator_bin_data(iter), record_len);
		len = record_len;
	} else {
		memcpy(data, bson_iterator_bin_data(iter), len);
	}

	bson_destroy(query);
	bson_destroy(response);
	return len;
}

int
mongo_db_update(uint64_t key, char *data, size_t len, db_type_t type)
{
	int ret = -1;
	bson b[1];
	char db_and_collection[NAME_SIZE];
	char record_name[REC_NAME_SIZE];

	bson_init(b);
	ret = bson_append_long(b, "record_number", key);
	if (ret != MONGO_OK) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to append record_num to bson for "
			"inode %"PRId64". Error = %d \n", __FUNCTION__, key, ret);
		bson_destroy(b);
		return -ret;
	}

	ret = construct_record_name(record_name, type);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid record type %d specified\n",
			__FUNCTION__, type);
		bson_destroy(b);
		return -1;
	}

	ret = bson_append_binary(b, record_name, BSON_BIN_BINARY,
		(const char *) data, len);
	if (ret != BSON_OK) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to append inode to bson for" 
			" inode %"PRId64". Error = %d \n", __FUNCTION__, key, ret);
		bson_destroy(b);
		return -ret;
	}

	ret = bson_append_long(b, "record_length", len);
	if (ret != BSON_OK) {
		sfslog(sfs_ctx, SFS_ERR, "%s: Failed to append size to bson for "
			"inode %"PRId64". Error = %d \n", __FUNCTION__, key, ret);
		bson_destroy(b);
		return -ret;
	}

	bson_finish(b);

	ret = construct_db_name(db_and_collection, type);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid type specified. db_type = %d\n",
			__FUNCTION__, type);
		bson_destroy(b);
		return -1;
	}

	pthread_mutex_lock(&mongo_db_mutex);
	ret = mongo_update(conn, db_and_collection, bson_shared_empty(), b,
			0, NULL);
	if (ret != MONGO_OK) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Record updation for key %"PRId64" into "
			"database %s failed with error %d\n",
			__FUNCTION__, key, db_and_collection, ret);
		pthread_mutex_unlock(&mongo_db_mutex);
		bson_destroy(b);
		return -ret;
	}
	pthread_mutex_unlock(&mongo_db_mutex);
	bson_destroy(b);

	return len;
}

int
mongo_db_delete(uint64_t key)
{
	int ret = -1;

	// For testing only. Will be done only in db_cleanup
	// Cleanup all collections
	ret = mongo_cmd_drop_collection(conn, DB_NAME, POLICY_COLLECTION, NULL);
	if (ret != MONGO_OK) {
		sfs_log(sfs_ctx, SFS_INFO,
			"%s: Collection %s does not exist in db %s\n",
			__FUNCTION__, POLICY_COLLECTION, DB_NAME);
		ret = -1;
		goto out;
	}
	ret = mongo_cmd_drop_collection(conn, DB_NAME, INODE_COLLECTION, NULL);
	if (ret != MONGO_OK) {
		sfs_log(sfs_ctx, SFS_INFO,
			"%s: Collection %s does not exist in db %s\n",
			__FUNCTION__, POLICY_COLLECTION, DB_NAME);
		ret = -1;
		goto out;
	}
	ret = mongo_cmd_drop_collection(conn, DB_NAME, JOURNAL_COLLECTION, NULL);
	if (ret != MONGO_OK) {
		sfs_log(sfs_ctx, SFS_INFO,
			"%s: Collection %s does not exist in db %s\n",
			__FUNCTION__, POLICY_COLLECTION, DB_NAME);
		ret = -1;
		goto out;
	}
	ret = mongo_cmd_drop_collection(conn, DB_NAME, CONFIG_COLLECTION, NULL);
	if (ret != MONGO_OK) {
		sfs_log(sfs_ctx, SFS_INFO,
			"%s: Collection %s does not exist in db %s\n",
			__FUNCTION__, POLICY_COLLECTION, DB_NAME);
		ret = -1;
	}

out:
	return ret;
}

int
mongo_db_cleanup(void)
{
	mongo_destroy(conn);

	return 0;
}
