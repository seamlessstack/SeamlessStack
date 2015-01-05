/*
 * Copyright (C) 2014 SeamlessStack
 *
 *  This file is part of SeamlessStack distributed file system.
 * 
 * SeamlessStack distributed file system is free software: you can redistribute
 * it and/or modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 * 
 * SeamlessStack distributed file system is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with SeamlessStack distributed file system. If not,
 * see <http://www.gnu.org/licenses/>.
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
typedef int (*db_get_t)(char * , char ** , db_type_t , log_ctx_t *);
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

extern db_t * create_db(void);
extern void destroy_db(db_t *);
extern void db_register(db_t *, db_init_t , db_open_t ,
	db_close_t , db_insert_t , db_remove_t ,
	db_iterate_t , db_get_t , db_seekread_t ,
	db_update_t , db_delete_t , db_cleanup_t , log_ctx_t *);

#endif // __SSTACK_DB_H__
