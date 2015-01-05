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

#ifndef __ROCKS_DB_H__
#define __ROCKS_DB_H__

#include <inttypes.h>
#include <sstack_db.h>
#include <sstack_log.h>
#define TRANSACTIONDB_NAME "/var/sfs/transaction_db"

#define T 1
#define WRITE_BUFFER_SIZE 67108864 // 64 MiB
#define MAXIMUM_OPEN_FILES 8192
#define MAX_WRITE_BUFFER_NUM 3
#define TARGET_FILE_SIZE_BASE 67108864
#define NUM_BLLOM_FILTER_BITS 10
#define LRU_CACHE_SIZE 67108864
#define MAX_COMPACTION_THREADS 8
#define MAX_BACKGROUND_FLUSH_THREADS 8


extern int rocks_db_open(log_ctx_t *);
extern int rocks_db_close(log_ctx_t *);
extern int rocks_db_init(log_ctx_t *);
extern int rocks_db_insert(char * , char * , size_t , db_type_t , log_ctx_t *);
extern int rocks_db_remove(char * , db_type_t , log_ctx_t *);
extern int rocks_db_seekread(char * , char *, size_t , off_t ,
		int , db_type_t , log_ctx_t *);
extern int rocks_db_update(char * , char * , size_t , db_type_t , log_ctx_t *);
extern void rocks_db_iterate(db_type_t , iterator_function_t , void *,
				log_ctx_t *);
extern int rocks_db_get(char * , char ** , db_type_t , log_ctx_t *);
extern int rocks_db_delete(char * , log_ctx_t *);
extern int rocks_db_cleanup(log_ctx_t *);

#endif // __ROCKS_DB_H__
