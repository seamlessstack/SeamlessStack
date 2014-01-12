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

#ifndef __SSTACK_DB_H__
#define __SSTACK_DB_H__

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdint.h>
#include <sstack_log.h>

// DB types
typedef enum {
	POLICY_TYPE = 1,	// For storing policy records
	INODE_TYPE  = 2,	// For storing inodes
	JOURNAL_TYPE = 3,	// For storing journal
	CONFIG_TYPE = 4,	// Fot storing config records
	MAX_TYPE = 5
} db_type_t;


//#define uint64_t unsigned long long int

typedef void (*iterator_function_t)(void *params,
				    char *key, void *data, ssize_t data_len);

// DB operations
typedef int (*db_init_t)(log_ctx_t *);
typedef int (*db_open_t)(log_ctx_t *);
typedef int (*db_close_t)(log_ctx_t *);
typedef int (*db_insert_t)(char * , char * , size_t, db_type_t , log_ctx_t *);
typedef int (*db_remove_t)(char * , db_type_t , log_ctx_t *);
typedef void (*db_iterate_t)(db_type_t db_type, iterator_function_t iterator,
			     void *params, log_ctx_t *);
typedef int (*db_get_t)(char * , char * , db_type_t , log_ctx_t *);
typedef int (*db_seekread_t)(char * , char * , size_t , off_t ,
		int , db_type_t, log_ctx_t *);
typedef int (*db_update_t)(char * , char * , size_t , db_type_t , log_ctx_t *);
typedef int (*db_delete_t)(char * , log_ctx_t *);
typedef int (*db_cleanup_t)(log_ctx_t *);


// DB operations
typedef struct db_operations {
	db_init_t db_init;
	db_open_t db_open;
	db_close_t db_close;
	db_insert_t db_insert;
	db_remove_t db_remove;
	db_iterate_t db_iterate;
	db_get_t db_get;
	db_seekread_t db_seekread;
	db_update_t db_update;
	db_delete_t db_delete;
	db_cleanup_t db_cleanup;
} db_ops_t;

// DB structure
typedef struct db {
	db_ops_t db_ops;
	log_ctx_t *ctx;
} db_t;

extern db_t *db;

// DB functions
static inline db_t *
create_db(void)
{
	db_t *db;

	db = malloc(sizeof(db_t));

	return db;
}

static inline void
destroy_db(db_t * db)
{
	if (db)
		free(db);
}

// DB registration function
static inline void
db_register(db_t *db, db_init_t db_init, db_open_t db_open,
	db_close_t db_close, db_insert_t db_insert, db_remove_t db_remove,
	db_iterate_t db_iterate, db_get_t db_get, db_seekread_t db_seekread,
	db_update_t db_update, db_delete_t db_delete, db_cleanup_t db_cleanup,
	log_ctx_t *ctx)
{
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

#endif // __SSTACK_DB_H__
