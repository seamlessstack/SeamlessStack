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

#ifndef __MONGO_DB_H__
#define __MONGO_DB_H__

#include <bson.h>
#include <mongo.h>
#include <sstack_db.h>
#include <sstack_log.h>
#define DB_NAME "sstack_db"
#define POLICY_COLLECTION "policy"
#define INODE_COLLECTION "inodes"
#define JOURNAL_COLLECTION "journal"
#define CONFIG_COLLECTION "config"
#define MONGO_IP	"127.0.0.1" // Will be updated with IP of host runnning DB
#define MONGO_PORT 27017
#define NAME_SIZE 32
#define REC_NAME_SIZE 32

extern int mongo_db_open(log_ctx_t *);
extern int mongo_db_close(log_ctx_t *);
extern int mongo_db_init(log_ctx_t *);
extern int mongo_db_insert(char * , char * , size_t , db_type_t , log_ctx_t *);
extern int mongo_db_seekread(char * , char *, size_t , off_t ,
		int , db_type_t , log_ctx_t *);
extern int mongo_db_update(char * , char * , size_t , db_type_t , log_ctx_t *);
extern void mongo_db_iterate(db_type_t , iterator_function_t , void *,
				log_ctx_t *);
extern int mongo_db_get(char * , char * , db_type_t , log_ctx_t *);
extern int mongo_db_delete(char * , log_ctx_t *);
extern int mongo_db_cleanup(log_ctx_t *);

static inline
int construct_db_name(char *name, db_type_t type)
{
	int ret = 0;

	memset(name, '\0', NAME_SIZE);
	strcpy(name, DB_NAME);
	strcat(name, ".");

	switch(type) {
		case POLICY_TYPE:
			strcat(name, POLICY_COLLECTION);
			break;
		case INODE_TYPE:
			strcat(name, INODE_COLLECTION);
			break;
		case JOURNAL_TYPE:
			strcat(name, JOURNAL_COLLECTION);
			break;
		case CONFIG_TYPE:
			strcat(name, CONFIG_COLLECTION);
			break;
		default:
			ret = -1;
	}

	return ret;
}

static inline
int construct_record_name(char *name, db_type_t type)
{
	int ret = 0;

	memset(name, '\0', REC_NAME_SIZE);

	switch(type) {
		case POLICY_TYPE:
			strcpy(name, "policy_struct");
			break;
		case INODE_TYPE:
			strcpy(name, "inode_struct");
			break;
		case JOURNAL_TYPE:
			strcpy(name, "journal_struct");
			break;
		case CONFIG_TYPE:
			strcpy(name, "config_struct");
			break;
		default:
			ret = -1;
	}

	return ret;
}
/*
 * New extension to mongo_create_capped_collection present in lib
 * Capped collections can not be sharded. So uncapped collections are required.
 *
 * Use size (in bytes) to preallocate space. Can provide 0.
 *
 * MongoDB does create collections implicitly. But not sure whether it creates
 * capped or uncapped collections. 
 * We can al this routine in db_open entry point along with mongo_client
 */

static inline
int mongo_create_collection( mongo *conn, const char *db,
	const char *collection, int size, bson *out )
{
	bson b[1];
	int result;

	bson_init( b );
	bson_append_string( b, "create", collection );
	bson_append_bool( b, "capped", 0 );
	bson_append_int( b, "size", size );
	bson_finish( b );

	result = mongo_run_command( conn, db, b, out );

	bson_destroy( b );

	return result;
}

#endif // __MONGO_DB_H__
