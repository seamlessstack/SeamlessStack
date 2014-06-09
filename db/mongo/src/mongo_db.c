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
#include <stdbool.h>
#include <mongo_db.h>
#include <sstack_log.h>
#include <unistd.h>

pthread_rwlock_t mongo_db_lock = PTHREAD_RWLOCK_INITIALIZER;
mongoc_client_t *client = NULL;


// db_open, db_close are NOP for this DB

int
mongo_db_open(log_ctx_t * ctx)
{
	char uristr[128] = { '\0' };

	sprintf(uristr, "mongodb://%s:%d", MONGO_IP, MONGO_PORT);

	// Initialize the connection
	client = mongoc_client_new(uristr);
	if (NULL == client) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to connect to mongodb. \n",
			 __FUNCTION__);
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
	char *mongo_ip = NULL;
	char uristr[128] = { '\0' };
	mongoc_collection_t *collection = NULL;
	mongoc_database_t *database = NULL;
	bool ret;

	// Get Mongo DB IP address from environment
	mongo_ip = getenv("MONGO_IPADDR");
	if (NULL == mongo_ip)
		sprintf(uristr, "mongodb://%s:%d", MONGO_IP, MONGO_PORT);
	else
		sprintf(uristr, "mongodb://%s:%d", mongo_ip, MONGO_PORT);

	// Initialize the connection
	client = mongoc_client_new(uristr);
	if (NULL == client) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to connect to mongodb. \n",
				__FUNCTION__);
		return -1;
	}

#ifdef MONGO_TEST
	// For testing only. Will be done only in db_cleanup
	// Cleanup all collections
	database = mongoc_client_get_database(client, DB_NAME);
	if (NULL == database) {
		sfs_log(ctx, SFS_INFO,
				"%s: Unable to find databse %s\n", __FUNCTION__, DB_NAME);

		return 0;
	}

	collection = mongoc_client_get_collection(client, DB_NAME,
			POLICY_COLLECTION);
	if (NULL == collection) {
		sfs_log(ctx, SFS_INFO,
			"%s: Collection %s does not exist in db %s\n",
			__FUNCTION__, POLICY_COLLECTION, DB_NAME);
	} else {
		bson_error_t error;

		ret = mongoc_collection_drop(collection,  &error);
		if (ret != true) {
			sfs_log(ctx, SFS_INFO,
					"%s: Unable to drop collection %s from db %s \n",
					__FUNCTION__, POLICY_COLLECTION, DB_NAME);
			mongoc_collection_destroy(collection);
		}
	}

	collection = mongoc_client_get_collection(client, DB_NAME,
			INODE_COLLECTION);
	if (NULL == collection) {
		sfs_log(ctx, SFS_INFO,
			"%s: Collection %s does not exist in db %s\n",
			__FUNCTION__, INODE_COLLECTION, DB_NAME);
	} else {
		bson_error_t error;

		ret = mongoc_collection_drop(collection,  &error);
		if (ret != true) {
			sfs_log(ctx, SFS_INFO,
					"%s: Unable to drop collection %s from db %s \n",
					__FUNCTION__, INODE_COLLECTION, DB_NAME);
			mongoc_collection_destroy(collection);
		}
	}

	collection = mongoc_client_get_collection(client, DB_NAME,
			JOURNAL_COLLECTION);
	if (NULL == collection) {
		sfs_log(ctx, SFS_INFO,
			"%s: Collection %s does not exist in db %s\n",
			__FUNCTION__, JOURNAL_COLLECTION, DB_NAME);
	} else {
		bson_error_t error;

		ret = mongoc_collection_drop(collection,  &error);
		if (ret != true) {
			sfs_log(ctx, SFS_INFO,
					"%s: Unable to drop collection %s from db %s \n",
					__FUNCTION__, JOURNAL_COLLECTION, DB_NAME);
			mongoc_collection_destroy(collection);
		}
	}

	collection = mongoc_client_get_collection(client, DB_NAME,
			CONFIG_COLLECTION);
	if (NULL == collection) {
		sfs_log(ctx, SFS_INFO,
			"%s: Collection %s does not exist in db %s\n",
			__FUNCTION__, CONFIG_COLLECTION, DB_NAME);
	} else {
		bson_error_t error;

		ret = mongoc_collection_drop(collection,  &error);
		if (ret != true) {
			sfs_log(ctx, SFS_INFO,
					"%s: Unable to drop collection %s from db %s \n",
					__FUNCTION__, CONFIG_COLLECTION, DB_NAME);
			mongoc_collection_destroy(collection);
		}
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
	bson_t b;
	bool ret = false;
	char record_name[REC_NAME_SIZE];
	bson_error_t error;
	mongoc_collection_t *collection = NULL;

	if (key == NULL) {
		sfs_log(ctx, SFS_ERR, "%s: Key passed is NULL. \n", __FUNCTION__);
		return -EINVAL;
	}

	bson_init(&b);
	ret = bson_append_utf8(&b, "record_num", strlen("record_num"), key,
			strlen(key));
	if (ret != true) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to append record_num to bson for "
			"inode %s. Error = %d \n", __FUNCTION__, key, ret);
		bson_destroy(&b);
		return -ret;
	}

	ret = construct_record_name(record_name, type);
	if (ret != 0) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid record type %d specified\n",
			__FUNCTION__, type);
		bson_destroy(&b);
		return -1;
	}


	ret = bson_append_int32(&b, "record_length", strlen("record_length"),
			(int32_t) len);
	if (ret != true) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to append size to bson for "
			"inode %s. Error = %d \n", __FUNCTION__, key, ret);
		bson_destroy(&b);

		return -1;
	}

	ret = bson_append_binary(&b, record_name, strlen(record_name),
			BSON_SUBTYPE_BINARY, (const uint8_t *) data, (uint32_t) len);
	if (ret != true) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to append inode to bson for"
			" inode %s. Error = %d \n", __FUNCTION__, key, ret);
		bson_destroy(&b);
		return -ret;
	}

	// bson_finish(&b);

	pthread_rwlock_wrlock(&mongo_db_lock);

	// Insert the document into collection
	collection = mongoc_client_get_collection(client, DB_NAME,
			get_collection_name(type));

	if (NULL != collection) {
		ret = mongoc_collection_insert(collection, MONGOC_INSERT_NONE, &b,
				NULL, &error);
		if (ret != true) {
			sfs_log(ctx, SFS_ERR,
				"%s: Failed to insert record %s of type %d"
				"into collection %s.%s . Error = %d\n",
				__FUNCTION__, key, (int) type, DB_NAME,
				get_collection_name(type), ret);
			bson_destroy(&b);
			pthread_rwlock_unlock(&mongo_db_lock);

			return -1;
		}
		sfs_log(ctx, SFS_INFO,
			"%s: Successfully inserted record %s of type %d"
			" into db.collection %s.%s\n",
			__FUNCTION__, key, (int) type, DB_NAME, get_collection_name(type));
		pthread_rwlock_unlock(&mongo_db_lock);
		bson_destroy(&b);

		return 1;
	} else {
		sfs_log(ctx, SFS_ERR, "%s: Collection %s not found. Insertion "
				"failed \n", __FUNCTION__, get_collection_name(type));
		pthread_rwlock_unlock(&mongo_db_lock);
		bson_destroy(&b);

		return -1;
	}
}


int
mongo_db_remove(char *key, db_type_t type, log_ctx_t *ctx)
{
	bson_t b;
	bool ret = false;
	char record_name[REC_NAME_SIZE];
	bson_error_t error;
	mongoc_collection_t *collection = NULL;

	if (key == NULL) {
		sfs_log(ctx, SFS_ERR, "%s: Key passed is NULL. \n", __FUNCTION__);
		return -EINVAL;
	}
	bson_init(&b);
	ret = bson_append_utf8(&b, "record_num", strlen("record_num"),
			key, strlen(key));
	if (ret != true) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to append record_num to bson for "
			"inode %s. Error = %d \n", __FUNCTION__, key, ret);
		bson_destroy(&b);

		return -1;
	}
	// bson_finish(&b);

	pthread_rwlock_wrlock(&mongo_db_lock);
	collection = mongoc_client_get_collection(client, DB_NAME,
			get_collection_name(type));
	if (NULL != collection) {
		ret = mongoc_collection_remove(collection,
				MONGOC_DELETE_SINGLE_REMOVE, &b, NULL, &error);
		if (ret != true) {
			sfs_log(ctx, SFS_ERR,
				"%s: Failed to remove record %s of type %d"
				"into collection %s.%s . Error = %d\n",
				__FUNCTION__, key, (int) type, DB_NAME,
				get_collection_name(type), ret);
			pthread_rwlock_unlock(&mongo_db_lock);
			bson_destroy(&b);

			return -1;
		}
		sfs_log(ctx, SFS_INFO,
			"%s: Successfully removed record %s of type %d"
			" into collection %s.%s\n",
			__FUNCTION__, key, (int) type, DB_NAME,
			get_collection_name(type));
		pthread_rwlock_unlock(&mongo_db_lock);
		bson_destroy(&b);

		return 1;
	} else {
		sfs_log(ctx, SFS_ERR, "%s: Collection %s not found. Unable "
				"to remove record %s \n",
				__FUNCTION__, get_collection_name(type), key);
		pthread_rwlock_unlock(&mongo_db_lock);
		bson_destroy(&b);

		return -1;
	}
}

// Seek to 'offset' from 'whence' and read data
// Uses gzseek and gzread
// Needed to read extents from inode
int
mongo_db_seekread(char * key, char *data, size_t len, off_t offset,
	int whence, db_type_t type, log_ctx_t *ctx)
{
	bool ret = false;
	bson_t query;
	const bson_t *response;
	bson_iter_t iter;
	char record_name[REC_NAME_SIZE];
	uint64_t record_len = 0;
	char *record;
	mongoc_collection_t *collection = NULL;
	mongoc_cursor_t *cursor = NULL;

	if (key == NULL) {
		sfs_log(ctx, SFS_ERR, "%s: Key passed is NULL. \n", __FUNCTION__);
		return -EINVAL;
	}

	if (NULL == data) {
		sfs_log(ctx, SFS_ERR, "%s: data pointer is NULL. \n",
			__FUNCTION__);
		return -EINVAL;
	}

	bson_init(&query);
	bson_append_utf8(&query, "record_num", strlen("record_num"), key,
			strlen(key));
	if (ret != true) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to append record_num to bson for "
			"inode %s. Error = %d \n", __FUNCTION__, key, ret);
		bson_destroy(&query);

		return -1;
	}
	// bson_finish(&query);

	// We expect only one document per collection for a given key
	pthread_rwlock_rdlock(&mongo_db_lock);

	collection = mongoc_client_get_collection(client, DB_NAME,
			get_collection_name(type));

	if (NULL != collection) {
		mongoc_read_prefs_t *read_prefs = NULL;
		bson_error_t error;
		bson_subtype_t subtype;
		uint32_t binary_len = 0;
		uint8_t *binary;
		bson_iter_t iter2;

		// Can also be MONGOC_READ_NEAREST if eventual consistency is preferred
		// as MongoDB follows primary-secondary model of replication which
		// leads to eventual consistency.
		read_prefs = mongoc_read_prefs_new(MONGOC_READ_PRIMARY);

		// Passing 1 as limit returns only one record
		// Passing NULL as fields retruns all fields of the matching record
		cursor = mongoc_collection_find(collection, MONGOC_QUERY_NONE,
							0, 1, 0, &query, NULL, read_prefs);

		mongoc_read_prefs_destroy(read_prefs);

		// Get the document
		ret = mongoc_cursor_next(cursor, (const bson_t **) &response);
		if (ret == false) {
			sfs_log(ctx, SFS_ERR, "%s: Unable to retrieve record from "
					"collection %s.%s. \n",
					__FUNCTION__, DB_NAME, get_collection_name(type));
			bson_destroy(&query);
			mongoc_cursor_destroy(cursor);
			pthread_rwlock_unlock(&mongo_db_lock);

			return -1;
		}
		pthread_rwlock_unlock(&mongo_db_lock);
		if (mongoc_cursor_error(cursor, &error)) {
			sfs_log(ctx, SFS_ERR,
				"%s: Failed to retrieve record for %s from"
				" collection %s.%s . Error = %d\n",
				__FUNCTION__, key, DB_NAME, get_collection_name(type), ret);
			bson_destroy(&query);
			mongoc_cursor_destroy(cursor);
		//	pthread_rwlock_unlock(&mongo_db_lock);

			return -1;
		}

		bson_iter_init(&iter, response);
		bson_iter_find_descendant(&iter, "record_length", &iter2);
		//(void)bson_iter_find(&iter, "record_length");
		record_len = bson_iter_int32(&iter);
		// Allocate memory for data
		record = malloc(record_len);
		if (NULL == record) {
			sfs_log(ctx, SFS_ERR,
				"%s: Failed to allocate memory for reading record."
				"Requested size = %"PRId64" key = %s db name = %s.%s\n",
				__FUNCTION__, record_len, key, DB_NAME,
				get_collection_name(type));
			bson_destroy(&query);
			// bson_iter_destroy(&iter);

			return -ENOMEM;
		}

		(void) construct_record_name(record_name, type);

		//(void)bson_iter_find(&iter,  record_name);
		bson_iter_find_descendant(&iter, record_name, &iter2);
		bson_iter_binary(&iter2, &subtype, &binary_len,
				(const uint8_t **) &binary);
		if (*binary)
			memcpy(record, binary, record_len);
		// Offset it by offset
		// Assuming whence to be SEEK_SET
		memcpy(data, record + offset, len);
		bson_destroy(&query);
		bson_destroy((bson_t *) response);
		mongoc_cursor_destroy(cursor);

		return len;
	} else {
		sfs_log(ctx, SFS_ERR, "%s: Unable to find collection %s.%s \n",
				__FUNCTION__, DB_NAME, get_collection_name(type));
		pthread_rwlock_unlock(&mongo_db_lock);
		bson_destroy(&query);

		return -1;
	}
}


void
mongo_db_iterate(db_type_t type, iterator_function_t iterator_fn, void *params,
				log_ctx_t *ctx)
{
	int ret = -1;
	bson_t *response;
	bson_iter_t iter;
	mongoc_cursor_t *cursor = NULL;
	char *key;
	uint64_t record_len;
	char *record_name;
	mongoc_collection_t *collection = NULL;
	const bson_t *doc;

	if (iterator_fn == NULL) {
		sfs_log(ctx, SFS_ERR, "%s: No iterator function supplied\n",
				__FUNCTION__);
		return;
	}

	collection = mongoc_client_get_collection(client, DB_NAME,
			get_collection_name(type));
	if (NULL != collection) {

		cursor = mongoc_collection_find (collection,
						MONGOC_QUERY_NONE, 0, 0, 0, response, NULL, NULL);

		while (mongoc_cursor_next(cursor, &doc)) {
			bson_subtype_t subtype;
			uint32_t binary_len = 0;
			uint8_t *binary;
			uint32_t len = 0;

			bson_iter_init(&iter, doc);
			bson_iter_find(&iter, "record_num");
			key = (char *)bson_iter_utf8(&iter, &len);
			construct_record_name(record_name, type);
			bson_iter_find(&iter, record_name);
			bson_iter_binary(&iter, &subtype, &binary_len,
				(const uint8_t **) &binary);
			bson_iter_find(&iter, "record length");
			record_len = bson_iter_int32(&iter);
			iterator_fn(params, key, binary, record_len);
			// bson_iter_destroy(&iter);
		}
		mongoc_cursor_destroy(cursor);
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
	bson_t query;
	const bson_t *response;
	bson_iter_t iter;
	char record_name[REC_NAME_SIZE];
	uint64_t record_len = 0;
	mongoc_collection_t *collection = NULL;
	mongoc_cursor_t *cursor = NULL;


	sfs_log(ctx, SFS_DEBUG, "%s: key = %s type = %d \n",
			__FUNCTION__, key, type);
	if (key == NULL) {
		sfs_log(ctx, SFS_ERR, "%s: Key passed is NULL. \n",
				__FUNCTION__);
		return -EINVAL;
	}

	bson_init(&query);
	bson_append_utf8(&query, "record_num", strlen("record_num"),
			key, strlen(key));
	// bson_finish(&query);

	// We expect only one document per collection for a given key
	// So mongo_find_one suffices
	pthread_rwlock_rdlock(&mongo_db_lock);
	collection = mongoc_client_get_collection(client, DB_NAME,
			get_collection_name(type));
	if (NULL != collection) {
		mongoc_read_prefs_t *read_prefs = NULL;
		bson_error_t error;
		bson_subtype_t subtype;
		uint32_t binary_len = 0;
		const uint8_t *binary;
		bson_t new;


		// Can also be MONGOC_READ_NEAREST if eventual consistency is preferred
		// as MongoDB follows primary-secondary model of replication which
		// leads to eventual consistency.
		// read_prefs = mongoc_read_prefs_new(MONGOC_READ_PRIMARY);

		// Passing 1 as limit returns only one record
		// Passing NULL as fields retruns all fields of the matching record
		cursor = mongoc_collection_find(collection, MONGOC_QUERY_NONE,
							0, 1, 0, &query, NULL, read_prefs);

		mongoc_read_prefs_destroy(read_prefs);
		if (mongoc_cursor_error(cursor, &error)) {
			sfs_log(ctx, SFS_ERR,
				"%s: Failed to retrieve record for %s from"
				" collection %s.%s . Error = %d\n",
				__FUNCTION__, key, DB_NAME, get_collection_name(type), ret);
			mongoc_cursor_destroy(cursor);
			pthread_rwlock_unlock(&mongo_db_lock);

			return -1;
		}
		// Get the document
		ret = mongoc_cursor_next(cursor, &response);
		if (ret == false) {
			sfs_log(ctx, SFS_ERR, "%s: Unable to retrieve record from "
					"collection %s.%s. \n",
					__FUNCTION__, DB_NAME, get_collection_name(type));
			mongoc_cursor_destroy(cursor);
			pthread_rwlock_unlock(&mongo_db_lock);

			return -1;
		}
		mongoc_cursor_destroy(cursor);
		pthread_rwlock_unlock(&mongo_db_lock);

		bson_iter_init(&iter, response);
		(void)bson_iter_find(&iter, "record_length");
		record_len = bson_iter_int32(&iter);
		sfs_log(ctx, SFS_DEBUG, "%s: record_len = %d \n",
				__FUNCTION__, record_len);
		syncfs(ctx->log_fd);
		// Allocate memory for data
		*data = malloc(record_len);
		if (NULL == *data) {
			sfs_log(ctx, SFS_ERR,
				"%s: Failed to allocate memory for reading record."
				"Requested size = %"PRId64" key = %s db name = %s.%s\n",
				__FUNCTION__, record_len, key, DB_NAME,
				get_collection_name(type));
			// bson_iter_destroy(&iter);

			return -ENOMEM;
		}

		(void) construct_record_name(record_name, type);
		sfs_log(ctx, SFS_DEBUG, "%s: record_name = %s\n", __FUNCTION__,
				record_name);

		syncfs(ctx->log_fd);

		bson_iter_init(&iter, response);
#if 0
		if (bson_iter_find(&iter,  record_name) != true) {
			sfs_log(ctx, SFS_DEBUG, "%s: Document does not contain record "
					"name %s\n", __FUNCTION__, record_name);

			return -1;
		}

		bson_iter_binary(&iter, &subtype, &binary_len,
				(const uint8_t **) &binary);
		memcpy(*data, bson_iter_value(&iter), record_len);
	//	if (*binary)
	//		memcpy(*data, binary, record_len);
#endif
#if 0
		bson_copy_to_excluding(response, &new, "record_num", "record_len");
		bson_iter_init(&iter, &new);
		bson_iter_binary(&iter, &subtype, &binary_len,
				(const uint8_t **) &binary);

		memcpy(*data, bson_iter_value(&iter), record_len);
//		memcpy(*data, binary, record_len);
#endif
#if 1
		BCON_EXTRACT((bson_t *) response, record_name, BCONE_BIN(subtype,
					binary, binary_len));
		memcpy(*data, binary, binary_len);

#endif
		bson_destroy((bson_t *) response);

		sfs_log(ctx, SFS_INFO, "%s: Retruning %d\n", __FUNCTION__, record_len);

		return record_len;
	} else {
		sfs_log(ctx, SFS_ERR, "%s: Unable to get collection %s.%s\n",
				__FUNCTION__, DB_NAME, get_collection_name(type));

		pthread_rwlock_unlock(&mongo_db_lock);
		return -1;
	}
}

int
mongo_db_update(char *key, char *data, size_t len, db_type_t type,
		log_ctx_t *ctx)
{
	bool ret = false;
	bson_t b;
	bson_t query;
	char record_name[REC_NAME_SIZE];
	mongoc_collection_t *collection = NULL;
	mongoc_cursor_t *cursor = NULL;
	bson_t reply;
	bson_error_t error;

	bson_init(&b);
	ret = bson_append_utf8(&b, "record_num", strlen("record_num"),
			key, strlen(key));
	if (ret != true) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to append record_num to bson for "
			"inode %s. Error = %d \n", __FUNCTION__, key, ret);
		bson_destroy(&b);

		return -1;
	}

	ret = construct_record_name(record_name, type);
	if (ret != 0) {
		sfs_log(ctx, SFS_ERR, "%s: Invalid record type %d specified\n",
			__FUNCTION__, type);
		bson_destroy(&b);

		return -1;
	}

	ret = bson_append_binary(&b, record_name,strlen(record_name),
			BSON_SUBTYPE_BINARY,
		(const uint8_t *) data, len);
	if (ret != true) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to append inode to bson for"
			" inode %s. Error = %d \n", __FUNCTION__, key, ret);
		bson_destroy(&b);

		return -1;
	}

	ret = bson_append_int32(&b, "record_length",
			strlen("record_length"), len);
	if (ret != true) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to append size to bson for "
			"inode %s. Error = %d \n", __FUNCTION__, key, ret);
		bson_destroy(&b);

		return -1;
	}

	// bson_finish(&b);
	bson_init(&query);
	ret = bson_append_utf8(&query, "record_num", strlen("record_num"),
			key, strlen(key));
	if (ret != true) {
		sfs_log(ctx, SFS_ERR, "%s: Failed to append record_num to bson for "
			"inode %s. Error = %d \n", __FUNCTION__, key, ret);
		bson_destroy(&query);

		return -1;
	}

	pthread_rwlock_wrlock(&mongo_db_lock);
	collection = mongoc_client_get_collection(client, DB_NAME,
			get_collection_name(type));
	if (NULL != collection) {
		ret = mongoc_collection_find_and_modify(collection,
				&query, NULL, &b, NULL, false, false, true, &reply, &error);
		if (ret == false) {
			sfs_log(ctx, SFS_ERR, "%s: Record updation for key %s into "
					"database %s.%s failed with error %d\n",
					__FUNCTION__, key, DB_NAME,
					get_collection_name(type), ret);
			pthread_rwlock_unlock(&mongo_db_lock);
			bson_destroy(&b);
			bson_destroy(&query);

			return -1;
		}
		sfs_log(ctx, SFS_INFO, "%s: Successfully updated record %s \n",
				__FUNCTION__, key);

		return len;
	} else {
		sfs_log(ctx, SFS_ERR, "%s: Collection %s.%s not found \n",
				__FUNCTION__, DB_NAME,
				get_collection_name(type));
		pthread_rwlock_unlock(&mongo_db_lock);
		bson_destroy(&b);
		bson_destroy(&query);

		return -1;
	}
}

int
mongo_db_delete(char *key, log_ctx_t *ctx)
{
	bool ret = false;
	mongoc_collection_t *collection = NULL;

	// For testing only. Will be done only in db_cleanup
	// Cleanup all collections
	collection = mongoc_client_get_collection(client, DB_NAME,
			POLICY_COLLECTION);
	if (NULL == collection) {
		sfs_log(ctx, SFS_INFO,
			"%s: Collection %s does not exist in db %s\n",
			__FUNCTION__, POLICY_COLLECTION, DB_NAME);
	} else {
		bson_error_t error;

		ret = mongoc_collection_drop(collection,  &error);
		if (ret != true) {
			sfs_log(ctx, SFS_INFO,
					"%s: Unable to drop collection %s from db %s \n",
					__FUNCTION__, POLICY_COLLECTION, DB_NAME);
			mongoc_collection_destroy(collection);
		}
	}

	collection = mongoc_client_get_collection(client, DB_NAME,
			INODE_COLLECTION);
	if (NULL == collection) {
		sfs_log(ctx, SFS_INFO,
			"%s: Collection %s does not exist in db %s\n",
			__FUNCTION__, INODE_COLLECTION, DB_NAME);
	} else {
		bson_error_t error;

		ret = mongoc_collection_drop(collection,  &error);
		if (ret != true) {
			sfs_log(ctx, SFS_INFO,
					"%s: Unable to drop collection %s from db %s \n",
					__FUNCTION__, INODE_COLLECTION, DB_NAME);
			mongoc_collection_destroy(collection);
		}
	}

	collection = mongoc_client_get_collection(client, DB_NAME,
			JOURNAL_COLLECTION);
	if (NULL == collection) {
		sfs_log(ctx, SFS_INFO,
			"%s: Collection %s does not exist in db %s\n",
			__FUNCTION__, JOURNAL_COLLECTION, DB_NAME);
	} else {
		bson_error_t error;

		ret = mongoc_collection_drop(collection,  &error);
		if (ret != true) {
			sfs_log(ctx, SFS_INFO,
					"%s: Unable to drop collection %s from db %s \n",
					__FUNCTION__, JOURNAL_COLLECTION, DB_NAME);
			mongoc_collection_destroy(collection);
		}
	}

	collection = mongoc_client_get_collection(client, DB_NAME,
			CONFIG_COLLECTION);
	if (NULL == collection) {
		sfs_log(ctx, SFS_INFO,
			"%s: Collection %s does not exist in db %s\n",
			__FUNCTION__, CONFIG_COLLECTION, DB_NAME);
	} else {
		bson_error_t error;

		ret = mongoc_collection_drop(collection,  &error);
		if (ret != true) {
			sfs_log(ctx, SFS_INFO,
					"%s: Unable to drop collection %s from db %s \n",
					__FUNCTION__, CONFIG_COLLECTION, DB_NAME);
			mongoc_collection_destroy(collection);
		}
	}
}


int
mongo_db_cleanup(log_ctx_t *ctx)
{
	mongo_db_delete(NULL, ctx);
	mongoc_client_destroy(client);
	sfs_log(ctx, SFS_INFO, "%s: DB %s destroyed \n", __FUNCTION__, DB_NAME);

	return 0;
}
