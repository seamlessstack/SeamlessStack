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

#ifndef __FILES_DB__
#define __FILES_DB__

#include <sstack_db.h>

extern int files_db_open(void);
extern int files_db_close(void);
extern int files_db_init(void);
extern int files_db_insert(uint64_t , char * , size_t , db_type_t);
extern int files_db_seekread(uint64_t , char *, size_t , off_t ,
		int , db_type_t);
extern int files_db_update(uint64_t , char * , size_t , db_type_t);
extern int files_db_get(uint64_t , char * , size_t , db_type_t);
extern int files_db_delete(uint64_t);
extern int files_db_cleanup(void);

#endif // __FILES_DB__

