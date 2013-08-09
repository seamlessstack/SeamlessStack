/* 
 * Type definitions for the BlueDS library
 * 
 * Copyright 2011 Shubhro Sinha
 * Author : Shubhro Sinha <shubhro22@gmail.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License. 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifndef _BDS_TYPES_H_
#define _BDS_TYPES_H_

#define offsetof(TYPE,MEMBER)	\
	((intptr_t)&(((TYPE*) 0)->MEMBER))

#define container_of(ptr,TYPE,MEMBER) 	\
	((TYPE*)((intptr_t)(ptr) - offsetof(TYPE,MEMBER)))

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

#define UNUSED_PARAMETER(parm) ((void)parm)

struct _list_head {
	struct _list_head *next;
	struct _list_head *prev;
};

typedef struct _list_head* bds_list_head_t;
typedef struct _list_head bds_int_list_t;
/* Basic types */
typedef int32_t bds_int;
typedef uint32_t bds_uint;
typedef int_fast64_t bds_long;
typedef uint_fast64_t bds_ulong;
typedef int16_t bds_status_t;
typedef int16_t bds_sint;
typedef uint16_t bds_usint;
#define BDS_LIST_POISON1 0xdeadbeaf
#define BDS_LIST_POISON2 0xbeafdead
#define BDS_DEF_PAGE_SIZE 4096
#define exch(a,b,t) (t=a,a=b,b=t)

#endif /*_BDS_TYPES_H_*/
