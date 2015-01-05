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
