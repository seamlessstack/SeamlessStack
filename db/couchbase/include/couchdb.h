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

#ifndef __COUCHDB_H__
#define __COUCHDB_H__

#include <sstack_db.h>

int couchdb_create(void);
int couchdb_get(uint64_t, char *, size_t);
int couchdb_seekread(uint64_t, char *, size_t, off_t, int);
int couchdb_update(uint64_t, char *, size_t);
int couchdb_insert(uint64_t, char *, size_t);
int couchdb_flush(void);
int couchdb_delete(uint64_t);
int couchdb_destroy(void);


#endif // __COUCHDB_H__
