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

#include <libcouchbase/couchbase.h>
#include <snappy-c.h>
#include <string.h>
#include <pthread.h>
#include <syslog.h>
#include <errno.h>
#include <stdlib.h>
#include <inttypes.h>
#include <couchdb.h>

#define DATA_SIZE	1024
#define MAX_KEY_SIZE	20
#define COUCHDB_LOG(format, ...)	printf(format, ##__VA_ARGS__)

lcb_t couchdb_handle = NULL;
void *value;
int num_bytes = 0;
pthread_rwlock_t db_rwlock;


/* Callbacks for the various database operations. Couchbase has a callback
   model for all its Database operations.

   Please refer to callback.h in libcouchbase directory for the descriptions of    each of these callbacks
*/
static void
error_cb(lcb_t instance, lcb_error_t error,
	const char *err_info)
{
	COUCHDB_LOG("Couchbase operation error %d due to %s\n", error, err_info);
}

static void get_cb(lcb_t instance, const void *cookie,
			lcb_error_t error, const lcb_get_resp_t *resp)
{
	if (error != LCB_SUCCESS) {
		COUCHDB_LOG("Error getting the entry for Key %"PRId64"",
			*(uint64_t *)(resp->v.v0.key));
		return;
	}
	USYSLOG(LOG_INFO, "%s: get cb %s %d\n",__FUNCTION__,
		(char *)(resp->v.v0.bytes), (int) resp->v.v0.nbytes);
	memcpy((char *)value, (char *)(resp->v.v0.bytes), resp->v.v0.nbytes);
	num_bytes =  resp->v.v0.nbytes;
}

static void
set_cb(lcb_t instance, const void *cookie, lcb_storage_t operation,
	lcb_error_t error, const lcb_store_resp_t *resp)

{
	if (error == LCB_SUCCESS) {
		COUCHDB_LOG("Successfully inserted value for key %"PRId64"\n",
			*(uint64_t *)(resp->v.v0.key));
	}
}

static void
remove_cb(lcb_t instance, const void *cookie, lcb_error_t error,
			const lcb_remove_resp_t *resp)

{
	if (error == LCB_SUCCESS) {
		COUCHDB_LOG("Successfully removed the key %"PRId64"\n",
			*(uint64_t *)(resp->v.v0.key));
	}
}

static void
flush_cb(lcb_t instance, const void *cookie,
			const char *server_endpt, lcb_error_t error)
{
	if (error == LCB_SUCCESS) {
		COUCHDB_LOG("Successfully cleared the database\n");
	}
}

/*
 * Name: couchdb_create
 * Description: This function creates an instance of the lcb.
 *		A connection to the cluster hosting the server node with
 *		the host ip "host" and bucket "vbucket" is established.
 *		All the operational callbacks are also registered
 *
 * Return val:	Returns the handle to the lcb instance to the caller
 */
/*opaque_t*/
int
couchdb_create(void)
{
	struct lcb_create_st create_opt;
	struct lcb_create_io_ops_st io_opts;
	lcb_error_t	ret;

	io_opts.version = 0;
	io_opts.v.v0.type = LCB_IO_OPS_DEFAULT;
	io_opts.v.v0.cookie = NULL;

	ret = lcb_create_io_ops(&create_opt.v.v0.io, &io_opts);
	if (ret != LCB_SUCCESS) {
		syslog(LOG_ERR, "%s:Failed to create IO instance: %s\n",
			__FUNCTION__, lcb_strerror(NULL, ret));
		return -1;
	}

	memset(&create_opt, 0, sizeof(struct lcb_create_st));
	create_opt.v.v0.host = "127.0.0.1:8091";
	create_opt.v.v0.user = "Administrator";
	create_opt.v.v0.passwd = "123456";
	create_opt.v.v0.bucket = "default";

	ret = lcb_create(&couchdb_handle, &create_opt);

	if (ret != LCB_SUCCESS) {
		COUCHDB_LOG("Error creating couchbase DB instance\n");
		return (-1);
	}

	/* Setup the Global variables */
	value = (void *) malloc(DATA_SIZE *  sizeof(char));
	if (NULL == value) {
		syslog(LOG_ERR, "%s: Failed to allocate memory for value\n",
			__FUNCTION__);
		return -ENOMEM;
	}
	pthread_rwlock_init(&db_rwlock, NULL);

	/* Once a valid handle is obtained, set up all the callbacks */
	lcb_set_error_callback(couchdb_handle, error_cb);
	lcb_set_get_callback(couchdb_handle, get_cb);
	lcb_set_store_callback(couchdb_handle, set_cb);
	lcb_set_flush_callback(couchdb_handle, flush_cb);
	lcb_set_remove_callback(couchdb_handle, remove_cb);

	/* Now connect to the cluster having the server node on the "host" ip */
	ret = lcb_connect(couchdb_handle);
	if (ret != LCB_SUCCESS) {
		COUCHDB_LOG("Error connecting to the cluster\n");
		return (-1);
	}

	/* Since the couchbase APIs are all asynchronous calls, we need
	   to block the thread until the connect succeeds */
	lcb_wait(couchdb_handle);

	return (0);
}

/*
 * Name:	couchdb_get
 * Input Args:
 *	handle - handle to the lcb instance
 *	key - key as in key-value pair
 *	data - value as in key-value pair
 *	len - data length
 *	lock - this is the place holder for key specific R/W lock. Right
 *		now, we are using a global database lock for synchronization
 *
 * Description: Gets the value for the given key and stores it in data field
 */
int
couchdb_get(uint64_t key, char *data, size_t len)
{
	char *db_key;
	lcb_size_t	nkey;
	lcb_error_t	ret;
	int	lock_result;
	lcb_get_cmd_t item;
	const lcb_get_cmd_t *const get_items[] = { &item };
	char *compressed_data = NULL, *uncompressed_data = NULL;
	size_t	compressed_len = 0, uncompressed_len = 0;
	int	res = 0;

	memset(&item, 0, sizeof(lcb_get_cmd_t));
	db_key = (char *) malloc(MAX_KEY_SIZE);
	if (NULL == db_key) {
		syslog(LOG_ERR, "%s: Unable to allocate memory for db_key\n",
			__FUNCTION__);
		return -ENOMEM;
	}
	sprintf(db_key,"%"PRId64"", key);
	nkey = strlen(db_key);

	item.v.v0.key = db_key;
	item.v.v0.nkey = nkey;


	/* Couchbase doesn't provide internal locking mechanism. We need
	   to handle the race conditions between various threads */
	lock_result = pthread_rwlock_rdlock(&db_rwlock);
	if (lock_result != 0) {
		syslog(LOG_ERR, "%s: Unable to obtain read lock. Error = %d\n",
			__FUNCTION__, errno);
		return -errno;
	}

	ret = lcb_get(couchdb_handle, NULL, 1, get_items);

	if (ret != LCB_SUCCESS) {
		syslog(LOG_INFO, "%s: lcb_mget failed with error  %d\n",
			__FUNCTION__,ret);
		free(data);
		data = NULL;
	}

	/* Block the thread till this operation completes */
	lcb_wait(couchdb_handle);

	lock_result = pthread_rwlock_unlock(&db_rwlock);

	compressed_data = value;
	compressed_len = num_bytes;
	if ((res = snappy_uncompressed_length(compressed_data, compressed_len,
			&uncompressed_len)) != SNAPPY_OK) {
		syslog(LOG_INFO, "%s: Corrupted data\n", __FUNCTION__);
		return (-res);
	}

	if (len > uncompressed_len) {
		syslog(LOG_ERR, "%s: partial read error\n",
			__FUNCTION__);
		return (-1);
	}

	uncompressed_data = (char *) malloc (uncompressed_len);
	if (uncompressed_data == NULL) {
		syslog(LOG_ERR, "%s: Unable to allocate memory for uncompressed_data\n", __FUNCTION__);
		return (-ENOMEM);
	}

	if ((res = snappy_uncompress((const char *)compressed_data,
 			compressed_len, uncompressed_data,
			&uncompressed_len)) == SNAPPY_OK) {
		memcpy((char *)data, (char *)uncompressed_data, len);
		free(uncompressed_data);
		return (len);
	}

	free(uncompressed_data);
	return (-res);
}

int
couchdb_seekread(uint64_t key, char *data, size_t len, off_t offset,
		int whence)
{
	char *db_key = NULL;
	lcb_size_t	nkey = 0;
	lcb_error_t	ret;
	int	lock_result = -1;
	lcb_get_cmd_t item;
	const lcb_get_cmd_t* const get_items[] = { &item };
	char *compressed_data = NULL, *uncompressed_data = NULL;
	size_t	compressed_len = 0, uncompressed_len = 0;
	int	res = 0;

	memset(&item, 0, sizeof(lcb_get_cmd_t));
	db_key = (char *) malloc(MAX_KEY_SIZE);
	if (NULL == db_key) {
		syslog(LOG_ERR, "%s: Failed to allocate memory for db_key\n",
			__FUNCTION__);
		return -ENOMEM;
	}
	sprintf(db_key,"%"PRId64"", key);
	nkey = strlen(db_key);

	item.v.v0.key = db_key;
	item.v.v0.nkey = nkey;


	/* Couchbase doesn't provide internal locking mechanism. We need
	   to handle the race conditions between various threads */
	lock_result = pthread_rwlock_rdlock(&db_rwlock);
	if (lock_result != 0) {
		syslog(LOG_ERR, "%s: Unable to obtain read lock. Error = %d \n",
			__FUNCTION__, errno);
		return -errno;
	}

	ret = lcb_get(couchdb_handle, NULL, 1, get_items);

	if (ret != LCB_SUCCESS) {
		free(data);
		data = NULL;
	}

	/* Block the thread till this operation completes */
	lcb_wait(couchdb_handle);

	lock_result = pthread_rwlock_unlock(&db_rwlock);

	compressed_data = value;
	compressed_len = num_bytes;
	if ((res = snappy_uncompressed_length(compressed_data, compressed_len,
			&uncompressed_len)) != SNAPPY_OK) {
		syslog(LOG_INFO, "%s: Corrupted data\n", __FUNCTION__);
		return (-res);
	}

	if (len > (uncompressed_len - offset)) {
		syslog(LOG_ERR, "%s: partial read error\n",
			__FUNCTION__);
		return (-1);
	}

	uncompressed_data = (char *) malloc(uncompressed_len);
	if (uncompressed_data == NULL) {
		syslog(LOG_ERR, "%s: Unable to allocate memory for "
			"uncompressed_data\n", __FUNCTION__);
		return (-ENOMEM);
	}

	if ((res = snappy_uncompress((const char *)compressed_data,
 			compressed_len, uncompressed_data,
			&uncompressed_len)) == SNAPPY_OK) {
		memcpy((char *)data, (char *)uncompressed_data + offset, len);
		free(uncompressed_data);
		return (len);
	}

	free(uncompressed_data);
	return (-res);
}

/*
 * Name:	couchdb_insert
 * Input Args:
 *	handle - handle to the lcb instance
 *	key - key as in key-value pair
 *	data - value as in key-value pair
 *	len - data length
 *	lock - this is the place holder for key specific R/W lock. Right
 *		now, we are using a global database lock for synchronization
 *
 * Description: Sets the value "data" for the given key
 */
int
couchdb_insert(uint64_t key, char *data, size_t len)
{
	char *db_key;
	lcb_size_t	key_size;
	lcb_error_t	ret;
	int	lock_result;
	lcb_store_cmd_t item;
	const lcb_store_cmd_t *const store_items[] = { &item };
	char *compressed_data = NULL;
	size_t	compressed_len = 0;
	int	res = 0;

	compressed_len = snappy_max_compressed_length(len);
	compressed_data = (char *) malloc(compressed_len);
	if (compressed_data == NULL) {
		syslog(LOG_ERR, "%s: Unable to allocate memory for "
			"compressed_data\n", __FUNCTION__);
		return (-ENOMEM);
	}

	if ((res = snappy_compress((const char *)data, len,
 			compressed_data, &compressed_len)) != SNAPPY_OK) {
		free(compressed_data);
		return (-res);
	}

	memset(&item, 0, sizeof(lcb_store_cmd_t));
	db_key = (char *) malloc(MAX_KEY_SIZE);
	if (NULL == db_key) {
		syslog(LOG_ERR, "%s: Failed to allocate memory for db_key\n",
			__FUNCTION__);
		return -ENOMEM;
	}
	sprintf(db_key,"%"PRId64"", key);
	USYSLOG(LOG_INFO, "%s: set1 %"PRId64" %s %s, \n",__FUNCTION__,  key,
		(char *) data, db_key);
	key_size = strlen(db_key);

	item.v.v0.key = db_key;
	item.v.v0.nkey = key_size;
	item.v.v0.bytes = compressed_data;
	item.v.v0.nbytes = compressed_len;
	item.v.v0.operation = LCB_ADD;


	lock_result = pthread_rwlock_wrlock(&db_rwlock);
	if (lock_result != 0) {
		syslog(LOG_ERR, "%s: Failed to get write lock error = %d \n",
			__FUNCTION__, errno);
		return -errno;
	}

	ret = lcb_store(couchdb_handle, NULL, 1, store_items);
	if (ret != LCB_SUCCESS) {
		syslog(LOG_ERR, "%s: lcb_store failed with error %s\n",
			__FUNCTION__, lcb_strerror(couchdb_handle, ret));
		return -ret;
	}

	/* Block the thread till this operation completes */
	lcb_wait(couchdb_handle);

	pthread_rwlock_unlock(&db_rwlock);

	return  (1);
}

int
couchdb_update(uint64_t key, char *data, size_t len)
{
	char *db_key = NULL;
	lcb_size_t	key_size;
	lcb_error_t	ret;
	int	lock_result = -1;
	lcb_store_cmd_t item;
	const lcb_store_cmd_t *const store_items[] = { &item };
	char *compressed_data = NULL;
	size_t	compressed_len = 0;
	int	res = 0;

	compressed_len = snappy_max_compressed_length(len);
	compressed_data = (char *)malloc(compressed_len);
	if (compressed_data == NULL) {
		syslog(LOG_ERR, "%s: Unable to allocate memory for compressed_data\n", __FUNCTION__);
		return (-ENOMEM);
	}

	if ((res = snappy_compress((const char *)data, len,
 			compressed_data, &compressed_len)) != SNAPPY_OK) {
		free(compressed_data);
		return (-res);
	}

	memset(&item, 0, sizeof(lcb_store_cmd_t));
	db_key = (char *) malloc(MAX_KEY_SIZE);
	if (NULL == db_key) {
		syslog(LOG_ERR, "%s: Unable to allocate memory for db_key\n",
			__FUNCTION__);
		return -ENOMEM;
	}
	sprintf(db_key,"%"PRId64"", key);
	USYSLOG(LOG_INFO, "%s: set1 %"PRId64" %s %s, \n", __FUNCTION__, key,
		(char *) data, db_key);
	key_size = strlen(db_key);

	item.v.v0.key = db_key;
	item.v.v0.nkey = key_size;
	item.v.v0.bytes = compressed_data;
	item.v.v0.nbytes = compressed_len;
	item.v.v0.operation = LCB_REPLACE;

	lock_result = pthread_rwlock_wrlock(&db_rwlock);
	if (lock_result != 0) {
		syslog(LOG_ERR, "%s: Failed to obtain write lock. Error = %d\n",
			__FUNCTION__, errno);
		return -errno;
	}

	ret = lcb_store(couchdb_handle, NULL, 1, store_items);
	if (ret != LCB_SUCCESS) {
		syslog(LOG_ERR, "%s: lcb_store failed with error %s\n",
			__FUNCTION__, lcb_strerror(couchdb_handle, ret));
		return -ret;
	}
	/* Block the thread till this operation completes */
	lcb_wait(couchdb_handle);

	pthread_rwlock_unlock(&db_rwlock);

	return  (1);

}

/*
 * Name:	couchdb_flush
 * Input Args:
 *	handle - handle to the lcb instance
 *
 * Description: Flushes the entire cluster
 */
int
couchdb_flush(void)
{
	lcb_error_t ret;
	int	lock_result;
	lcb_flush_cmd_t *cmd = malloc(sizeof(*cmd));
	const lcb_flush_cmd_t *const flush_cmd[] = { cmd };

	cmd->version = 0;

	lock_result = pthread_rwlock_wrlock(&db_rwlock);
	if (lock_result != 0) {
		syslog(LOG_ERR, "%s: Failed to obtain writelock. Error = %d\n",
			__FUNCTION__, errno);
		return -errno;
	}

	ret = lcb_flush(couchdb_handle, NULL, 1, flush_cmd);
	if (ret != LCB_SUCCESS) {
		syslog(LOG_ERR, "%s: lcb_flush failed with error %s\n",
			__FUNCTION__, lcb_strerror(couchdb_handle, ret));
		return -ret;
	}

	/* Block the thread till this operation completes */
	lcb_wait(couchdb_handle);

	pthread_rwlock_unlock(&db_rwlock);
	USYSLOG(LOG_INFO, "\n%s: lflushed siuccess\n", __FUNCTION__);

	return (1);
}

/*
 * Name:	couchdb_delete
 * Input Args:
 *	handle - handle to the lcb instance
 *	key - key as in key-value pair
 *	lock - this is the place holder for key specific R/W lock. Right
 *		now, we are using a global database lock for synchronization
 *
 * Description: deletes the entry referred by the given key
 */
int
couchdb_delete(uint64_t key)
{
	char *db_key;
	size_t	key_size;
	lcb_error_t    ret;
	int	lock_result;
	lcb_remove_cmd_t item;
	const lcb_remove_cmd_t *const del_items[] = { &item };

	memset(&item, 0, sizeof(lcb_remove_cmd_t));
	db_key = (char *) malloc(MAX_KEY_SIZE);
	if (NULL == db_key) {
		syslog(LOG_ERR, "%s: Unable to allocate memory for db_key \n",
			__FUNCTION__);
		return -ENOMEM;
	}
	sprintf(db_key,"%"PRId64"", key);
	key_size = strlen(db_key);
	lock_result = pthread_rwlock_wrlock(&db_rwlock);
	if (lock_result != 0) {
		syslog(LOG_ERR, "%s: Unable to obtain write lock. Error = %d\n",
			__FUNCTION__, errno);
		return -errno;
	}

	item.v.v0.key = db_key;
	item.v.v0.nkey = key_size;


	ret = lcb_remove(couchdb_handle, NULL, 1, del_items);
	if (ret != LCB_SUCCESS) {
		syslog(LOG_ERR, "%s: lcb_remove failed with error %s\n",
			__FUNCTION__, lcb_strerror(couchdb_handle, ret));
		return -ret;
	}

	/* Block the thread till this operation completes */
	lcb_wait(couchdb_handle);

	pthread_rwlock_unlock(&db_rwlock);

	return (1);
}

/*
 * Name:	couchdb_destroy
 * Input Args:
 *	handle - handle to the lcb instance
 *
 * Description: Destroys the lcb instance
 */
int
couchdb_destroy(void)
{

	couchdb_flush();

	lcb_destroy(couchdb_handle);

	return (1);
}

