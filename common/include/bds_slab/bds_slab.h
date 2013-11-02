/*
 * SLAB based allocator for BDS library.
 * For more information please please visit
 * blueds.shubhalok.com
 * ----------------------
 * Copyright 2011 Shubhro Sinha
 * Author : Shubhro Sinha <shubhro@shubhalok.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; Version 2 of the License.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _BDS_SLAB_H_
#define _BDS_SLAB_H_

#include <stdint.h>
#include <pthread.h>
#include "bds_types.h"
#include "bds_sys.h"
#include "bds_atomic.h"

#define BDS_CACHE_NAME_MAX	32
#define	CFS_CONFIG_OFF_SLAB	0x00000001;
#define ARRAY_CACHE_ENTRIES	10
#define BDS_ERR_NOT_FOUND	0xedfa
#define BDS_OBJECT_FOUND	~(BDS_ERR_NOT_FOUND)
#define BDS_FOUND		BDS_OBJECT_FOUND
#define BDS_NOT_FOUND		BDS_ERR_NOT_FOUND
#define BDS_BUFCTL(arg) (((intptr_t)arg) + sizeof(struct _slab_desc))
#define BDS_MAX_NR_CPUS		256

typedef uint16_t bds_bufctl_t;
struct _array_cache_desc {
	bds_int	available;	/* Number of object available */
	bds_int	limit;		/* Limit*/
	bds_int	batchcount;	/* Number of object to be allocated in one batch */
	int_fast16_t	touched;	/* Indicates whether the cache is used or not */
	pthread_spinlock_t	lock;		/* Lock */
	void		*entry[]; 	/* The LIFO array of objects */
};

typedef struct _array_cache_desc array_cache_desc;

struct _slab_list {
	/* The slab lists. */
	bds_int_list_t	slabs_partial, slabs_full, slabs_free;
	/* Number of free object in the slab;
	   Limit of free objects available in slab */
	bds_int	free_objects, free_limit;
	bds_int	colour_next;	/* The next colour to be allocated */
	pthread_mutex_t partial_list_lock, full_list_lock, free_list_lock;

};

typedef	struct _slab_list bds_slab_list;

struct _cache_desc {
	/* Cache tunables;
	   Same as values in array_cache_desc. Protected by cache_chain_lock */
	bds_int	batchcount,limit,shared;

	bds_int  flags; /* Cache static and dynamic flags*/
	bds_uint num;   /* Number of objects per slab */
	bds_sint colour;/* Cache colouring range */
	bds_sint colour_offset;/* Cache colouring offset */
	bds_sint colour_next; /* Next available colour */
	bds_int page_order; /* Size of cache in order of pages */
	bds_int_list_t	next; /* Next cache in the cache chain */
	bds_int slab_size;  /* Size of each slab */
	bds_int mgmt_size;  /* Size of the management data */
	size_t	buffer_size;  /* Size of each object */
	bds_uint slab_magic;  /* SLAB magic for the current slab */
	pthread_spinlock_t cache_lock;  /* Lock for cache desc structure */
	atomic_t refcount;    /* The reference count */
	atomic_t free_slab_count, total_slab_count;
	bds_uint linger; /* Minimum number of free slabs to be present in the cache */

	/* Pointer to the constructor and desctructor function : Can be NULL */
	void (*ctor)(void *obj);
	void (*dtor)(void *obj);
	char	name[BDS_CACHE_NAME_MAX]; /* Name of the cache */

	/* Offset where the first object
	   starts; size of each object in the cache */
	bds_int obj_offset, obj_size;
	bds_slab_list slab_list;
	/* Per CPU list of available objects.
	   Number of rows = Number of CPUs in the system
	 * Determined at run time*/
#ifdef BDS_CONFIG_SMP
	array_cache_desc *array_cache[0];
#endif
};


typedef struct _cache_desc* bds_cache_desc_t;

struct _slab_desc {
	bds_uint slab_magic;		/* Slab magic */
	bds_int_list_t	next;		/* The next slab in the list */
	bds_int colour_offset;		/* Offset to the first object */
	void* s_mem;			/* Pointer to the slab memory */
	bds_int free;	/* Pointer to the first free object in the slab */
	bds_cache_desc_t parent_cache;	/* Pointer to the cache
					   desc to which this slab belongs to */
	atomic_t refcount;  /* The number of active objects in the slab */
	pthread_mutex_t slab_lock;
};

typedef struct _slab_desc* bds_slab_desc_t;

/* Function declarations for the SLAB allocator */
bds_status_t bds_cache_create (const char *name, size_t size,
		bds_int flags, void (*ctor)(void*),
			       void (*dtor)(void *),
			       bds_cache_desc_t *cache_pp);
void bds_cache_destroy (bds_cache_desc_t cache_p, bds_int force);
void bds_cache_shrink (bds_cache_desc_t cache_p);
void* bds_alloc (size_t size);
void bds_free (void *ptr);
void bds_cache_stats (bds_cache_desc_t cache_p);
void* bds_cache_alloc (bds_cache_desc_t cache_p);
void* bds_alloc_chunk (bds_cache_desc_t cache_p, bds_int *nr_objs);
void bds_cache_free (bds_cache_desc_t cache_p, void *obj);

#endif
