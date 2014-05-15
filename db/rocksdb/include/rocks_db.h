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

#ifndef __ROCKS_DB_H__
#define __ROCKS_DB_H__

#include <sstack_db.h>
#include <sstack_log.h>
#define TRANSACTIONDB_NAME "transaction_db"

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
