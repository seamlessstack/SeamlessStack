/* SLAB allocator for the BDS library.
 * For more information please visit
 * blueds.shubhalok.com
 * --------------------
 * Copyright 2012 Shubhro Sinha
 * Author : Shubhro Sinha <shubhro22@gmail.com>
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
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <limits.h>
#include <errno.h>
#include <bds_slab/bds_types.h>
#include <bds_slab/bds_sys.h>
#include <bds_slab/bds_list.h>
#include <bds_slab/bds_slab.h>
#ifdef _LINUX_
#include <sys/mman.h>
#include <pthread.h>
#endif
#define bds_error printf
#define bds_info printf 
#define _GNU_SOURCE_
#define SLAB_CTOR_DESTRUCTOR	0x00000002UL
#define CFLGS_OFF_SLAB		0x80000000UL
#define OFF_SLAB(x) (x->flags & CFLGS_OFF_SLAB)
#define SLAB_LIMIT (~(0U) - 3)
/* Slab allocator suports upto 32 megabytes of memory
 * for bds_alloc
 * */
#define BDS_ALLOC_SHIFT_HIGH 25
#define BDS_ALLOC_MAX_ORDER (BDS_ALLOC_SHIFT_HIGH - __getpageshift()) 
#define BDS_SLAB_BREAK_ORDER_HIGH 2;
#define BOOT_CPU_CACHE_ENTRIES 1
//extern bds_system_info_t g_sys_info;
struct arraycache_init {
	struct _array_cache_desc cache;
	void *entries[BOOT_CPU_CACHE_ENTRIES];
};
//static struct arraycache_init initarray_cache;
//static bds_int bds_slab_break_order = BDS_SLAB_BREAK_ORDER_HIGH;
static bds_int_list_t cache_chain;
static bds_mutex_t cache_chain_lock = PTHREAD_MUTEX_INITIALIZER; 
/* Internal cache of cache descriptors */
#define cache_cache cache_cache_w.cache
struct cache_cache_desc {
	struct _cache_desc cache;
	array_cache_desc array_desc[BDS_MAX_NR_CPUS];
};
static struct cache_cache_desc cache_cache_w = {
	{
		.batchcount = 1,
		.shared = 1,
		.name = "bds_cache_cache",
	}
};
#ifndef BDS_ALLOC_MAX_SIZE 
#define BDS_ALLOC_MAX_SIZE 8192 
#endif
struct alloc_size {
	size_t size;
	bds_cache_desc_t cache_p;
};
static struct alloc_size alloc_sizes[] = {
	{32,NULL},
	{64,NULL},
	{96,NULL},
	{128,NULL},
	{192,NULL},
	{256,NULL},
	{512,NULL},
	{1024,NULL},
	{2048,NULL},
	{4096,NULL},
	{8192,NULL},
#if BDS_ALLOC_MAX_SIZE > 8192
	{16384,NULL},
	{32768,NULL},
	{65536,NULL},
	{131072,NULL},
#endif
#if BDS_ALLOC_MAX_SIZE >= 262144
	{262144,NULL},
#endif
#if BDS_ALLOC_MAX_SIZE >= 524288
	{524288,NULL},
#endif
#if BDS_ALLOC_MAX_SIZE >= 524288
	{524288,NULL},
#endif
#if BDS_ALLOC_MAX_SIZE >= 1048576 
	{104856,NULL},
#endif
#if BDS_ALLOC_MAX_SIZE >= 2097152 
	{2097152,NULL},
#endif
#if BDS_ALLOC_MAX_SIZE >= 4194304 
	{4194304,NULL},
#endif
#if BDS_ALLOC_MAX_SIZE >= 8388608 
	{8388608,NULL},
#endif
#if BDS_ALLOC_MAX_SIZE >= 16777216 
	{16777216,NULL},
#endif
#if BDS_ALLOC_MAX_SIZE >= 33554432 
	{33554432,NULL},
#endif
	{ULONG_MAX,NULL},
};

/* Function to return page aligned memory.
 * @size: Amount of memory to be allocted. Rounded off to the 
 * nearest page
 */
static void* __get_free_pages(size_t num_pages)
{
	void *ptr;
#ifdef BDS_SLAB_DEBUG
	void *error;
#endif

#ifdef _LINUX_
	size_t page_size = getpagesize();
#ifdef BDS_SLAB_DEBUG
	error = MAP_FAILED;
#endif
	ptr = mmap (NULL, num_pages * page_size, 
		PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if (ptr == MAP_FAILED) {
		bds_error ("No pages available\n");
		return NULL;
	}
#else
#ifdef BDS_SLAB_DEBUG
	error = NULL;
#endif
	ptr = malloc (num_pages * BDS_DEF_PAGE_SIZE);
	if (ptr == NULL) {
		bds_error ("Unable to allocated memory\n");
	}
#endif
#ifdef BDS_SLAB_DEBUG
	assert (ptr != error);
#endif
	return ptr;
}
/**
 *__reclaim_free_pages: Return mapped pages to the system
 * params:
 * @pages: The start address
 * @nr_pages: The number of pages to be unmapped
 */
static inline 
void __reclaim_free_pages(void *pages, bds_int nr_pages)
{
	size_t length = nr_pages * getpagesize();
	if (munmap(pages, length) < 0)
		bds_error("Error reclaiming memory\n");

}

/* Get a bds_cache_desc from cache_cache */
static inline 
bds_cache_desc_t __allocate_bds_cache_desc(void)
{
	bds_cache_desc_t cache_p;
	cache_p = bds_cache_alloc(&cache_cache);
	return cache_p;
}

/* Find "name" in the cache_chain. If found return FOUND */
bds_int __find_cache(const char *name)
{

	bds_list_head_t head = cache_chain.next;
	bds_cache_desc_t cache_p;
	/* Find out whether the cache exists or not. 
	 *  Must hold cache_chain_lock */
	bds_mutex_lock(&cache_chain_lock);
	for (head = cache_chain.next; head != &cache_chain; head = head->next) {
		cache_p = container_of (head, struct _cache_desc, next);
		if (!strncmp(cache_p->name,name, BDS_CACHE_NAME_MAX)) {
			bds_mutex_unlock(&cache_chain_lock);
			return BDS_FOUND;
		}
	}
	bds_mutex_unlock(&cache_chain_lock);
	return BDS_NOT_FOUND;
}

/* Find whether an object belongs to a slab list. Return FOUND if found */
bds_int __find_object_in_slab_list (bds_list_head_t list, 
		void *obj, bds_slab_desc_t *slab)
{
	bds_list_head_t head;
	bds_slab_desc_t slab_p;
	bds_cache_desc_t cache_p;
	if (list_empty(list))
		return BDS_ERR_NOT_FOUND;
	/* Iterate over the list of the caches present in the system */
	for (head = list->next; head != list; head = head->next) {
		slab_p = container_of (head, struct _slab_desc, next);
		cache_p = slab_p->parent_cache;
		/* Check whether the address passed belongs to this
		   slab or not */
		if (obj >= slab_p->s_mem && obj <= slab_p->s_mem + 
				(cache_p->num * cache_p->buffer_size)) {
			*slab = slab_p;
			return BDS_FOUND;
		}
	}
	return BDS_NOT_FOUND;
}

/* Find a general cache suitable for @size */
bds_cache_desc_t __find_general_cache(size_t size)
{
	struct alloc_size *csize = alloc_sizes;
	/* No size specified */
	if (!size)
		return NULL;
	while (size > csize->size)
		csize++;
	return csize->cache_p;

}
/* _cache_estimate: Estimate the number of objects per slab for a given slab size 
 * @page_order: size in order of pages
 * @buffer_size: size of each object
 * @flags: cacge flags
 * @*left_over: bytes left over per slab (o/p)
 * @*num: number of objects per slab (o/p)
 */
static void cache_estimate (bds_int page_order, size_t buffer_size, 
		bds_int flags, size_t *left_over, bds_uint *num)
{
	bds_uint nr_objs;
	size_t mgmt_size;
	size_t slab_size = getpagesize() << page_order;
	/* The slab management structure can be off slab or on it.
	 * In the latter case the the slab  memory is 
	 * used for the following:
	 * 	- slab_desc_t
	 * 	- on bds_bufctl for each object
	 * 	- @buffer_size bytes for each object
	 */
	if (flags & CFLGS_OFF_SLAB) {
		mgmt_size = 0;
		nr_objs = slab_size / buffer_size;

		if (nr_objs > SLAB_LIMIT)
			nr_objs = SLAB_LIMIT;
	} else {
		nr_objs = (slab_size - sizeof(struct _slab_desc)) / 
			(buffer_size + sizeof(bds_bufctl_t));
		if (nr_objs > SLAB_LIMIT)
			nr_objs = SLAB_LIMIT;
		mgmt_size = sizeof (struct _slab_desc) + 
			nr_objs * sizeof(bds_bufctl_t);
	}
	*num = nr_objs;
	*left_over = slab_size - (nr_objs * buffer_size + mgmt_size);
}

/**
 * _calculate_slab_order: Calculate the size of each slab
 * @cache_p : Pointer to the cache
 * @buffer_size: Size of each buffer
 * @flags: Cache flags
 */
static size_t calculate_slab_order (bds_cache_desc_t cache_p, 
		size_t buffer_size, bds_uint flags)
{
	bds_uint offslab_limit = 0;
	size_t left_over = 0;
	bds_uint page_order;
	bds_uint page_size = getpagesize();
	bds_uint bds_alloc_max_order = BDS_ALLOC_SHIFT_HIGH - __getpageshift();
	for (page_order = 0; page_order <= bds_alloc_max_order; page_order++) {
		bds_uint num;
		size_t remainder;
		cache_estimate(page_order, buffer_size, 
				flags, &remainder, &num);
		if (!num)
			continue;
		/* Found something acceptable. Save it */
		cache_p->num = num;
		cache_p->page_order = page_order;
		cache_p->slab_size = (page_size << page_order);
		left_over = remainder;

		if (left_over * 16 <= (page_size << page_order)) 
			break;

		if (flags & CFLGS_OFF_SLAB) {
			/* Maximum number of objects per slabs when off slab slab
			 * management structures are present */
			offslab_limit = buffer_size - sizeof (bds_slab_desc_t);
			offslab_limit /= sizeof (bds_bufctl_t);

			if (num > offslab_limit)
				break;
		}
	}
	return left_over;
}

/**
 * __setup_cache_desc: Setup the cache descriptor.
 * params:
 * @cache_p: pointer to cache_desc
 * @flags: The flags passed
 * @size: The size of each object 
 */
bds_status_t 
__setup_cache_desc (bds_cache_desc_t cache_p, bds_int flags, size_t size)
{
	size_t mgmt_size;
	size_t page_size = getpagesize();
	bds_uint left_over;
	/* Determine whether slab descriptor should sit on slab or off it */
	if (size >= page_size >> 3) {
		flags |= CFLGS_OFF_SLAB;
	}
	/* Find the appropriate slab size */
	left_over = calculate_slab_order (cache_p, size, flags);

	if (!cache_p->num)
		return -ENOMEM;
	mgmt_size = sizeof(bds_bufctl_t)*cache_p->num 
		+ sizeof(struct _slab_desc);
	cache_p->mgmt_size = mgmt_size;
	/* If the slab is off slab and we have enough space make it 
	 * on slab :) at the cost of any extra colouing :(*/
	if (flags & CFLGS_OFF_SLAB && left_over >= mgmt_size) {
		flags &= ~(CFLGS_OFF_SLAB);
		left_over -= mgmt_size;
	}
	cache_p->flags = flags;
	cache_p->buffer_size = size;
	cache_p->colour_offset = __getcachelinesize();
	cache_p->colour = left_over / cache_p->colour_offset;
	if (!cache_p->colour)
		cache_p->colour_offset = 0;
	/* In future change it to a random number */
	cache_p->slab_magic = 0xdeadbeef;
	bds_spin_init(&cache_p->cache_lock);
	/* Initialize bds slab list and its lock*/
	INIT_LIST_HEAD (&cache_p->slab_list.slabs_partial);
	INIT_LIST_HEAD (&cache_p->slab_list.slabs_full);
	INIT_LIST_HEAD (&cache_p->slab_list.slabs_free);
	bds_mutex_init(&cache_p->slab_list.list_lock);
	return 0;
}
/* bds_cache_create: Creates a BDS cache.
 * @params:
 * 	name: Name of the cache ( < 32 chars)
 * 	size: Size of each object
 * 	flags: Static flags
 * 	ctor, dtor: The constructor and destructor functions
 */
bds_status_t bds_cache_create (const char *name, size_t size,
		bds_int flags, void(*ctor)(void *), void(*dtor)(void *), 
		bds_cache_desc_t *cache_pp)
{
	bds_cache_desc_t cache_p;
	/* Check whether the cache already exists or not */
	if (__find_cache(name) == BDS_FOUND)
		return -EEXIST;

	cache_p = __allocate_bds_cache_desc();
	if (!cache_p) 
		return -ENOMEM;
	/*if (flags & SLAB_CTOR_DESTRUCTOR) 
		dtor = ctor;*/
	if (__setup_cache_desc(cache_p, flags, size) < 0)
		return -ENOMEM;

	/* This is going to change when the debug f/w is installed*/
	cache_p->ctor = ctor;
	cache_p->dtor = dtor;
	strncpy (cache_p->name, name, BDS_CACHE_NAME_MAX);
	/* Set up the cpu cache */
	/* Not doing smp part for now */
	/* Cache created : Add to list, Hold the cache_chain_lock */
	bds_list_add (&cache_p->next, &cache_chain);
	bds_mutex_unlock(&cache_chain_lock);
	*cache_pp = cache_p;
	return 0;
}

/** 
 * bds_cache_init: Initialize the BDS slab allocator
 */

__attribute__((constructor))
void bds_cache_init (void)
{
	bds_int order, mgmt_size;
	//bds_uint i;
	size_t left_over = 0;
	//char name[BDS_CACHE_NAME_MAX];
	//bds_cache_desc_t cache_p = NULL;
	bds_int bds_alloc_max_order = BDS_ALLOC_SHIFT_HIGH - __getpageshift();
	/* Initialization is tricky as we have to allocate a lot
	 * of things from cache before even cache exists. The things
	 * to be done as part of initialization are as follows
	 * 
	 * 1) Initialize cache_cache
	 * 2) Create the kmalloc caches.
	 * 3) Create the slab descriptor cache.
	 */
	//bds_generate_sysinfo();
	INIT_LIST_HEAD (&cache_chain);
	bds_spin_init(&cache_cache.cache_lock);
	cache_cache.colour_offset = __getcachelinesize();
#ifdef BDS_CONFIG_SMP
	cache_cache.buffer_size = sizeof (struct _cache_desc) + 
		(NR_CPUS * sizeof (struct _array_cache_desc));
#else 
	cache_cache.buffer_size = sizeof (struct _cache_desc);
#endif
	for (order = 0; order < bds_alloc_max_order; order++) {
		cache_estimate (order, cache_cache.buffer_size, 0, 
				&left_over, &cache_cache.num);
		if (cache_cache.num)
			break;
	}
	assert(cache_cache.num != 0);
	mgmt_size = sizeof(bds_bufctl_t)*cache_cache.num 
		+ sizeof(struct _slab_desc);
	cache_cache.mgmt_size = mgmt_size;
	/* In future change it to a random number */
	cache_cache.slab_magic = 0xdeadbeef;
	/* Initialize bds slab list and its lock*/
#ifdef BDS_CONFIG_SMP
	for (i=0; i<NR_CPUS; ++i) 
		cache_cache.array_cache[i] = &initarray_cache;
#endif
	/* TODO: Take care of the slab lists. 
	   Create a cache for initial allocations*/
	INIT_LIST_HEAD (&cache_cache.slab_list.slabs_partial);
	INIT_LIST_HEAD (&cache_cache.slab_list.slabs_full);
	INIT_LIST_HEAD (&cache_cache.slab_list.slabs_free);
	bds_mutex_init(&cache_cache.slab_list.list_lock);
	bds_list_add (&cache_cache.next, &cache_chain);
	/* Create the cache for the array cache */
#ifdef BDS_CONFIG_SMP
	
#endif
	/* Create the malloc caches */
#if 0
	size_t csize = sizeof (alloc_sizes)/sizeof(struct alloc_size) - 1;
	for (i=0; i < csize; ++i) {
		sprintf (name,"bds-alloc-%u", (unsigned)alloc_sizes[i].size);	
		printf ("Allocating for: %d\n", alloc_sizes[i].size);
		bds_cache_create (name, alloc_sizes[i].size,
				0,NULL,NULL, &cache_p);
		alloc_sizes[i].cache_p = cache_p;
	}
#endif
	return;
	printf ("Cache init done..");
}

/**
  * Given the slab descriptor memory setup the values for the slab descriptor
  * variables. Also set up the bufctls. 
  * @params:
  *	cache_p : Pointer to the cache to which this slab belongs
  *	slab_p : Pointer to the slab management data structure
  */
static void 
__setup_slab_desc(bds_cache_desc_t cache_p, void* mgmt_mem, void *s_mem)
{
	bds_slab_desc_t slab_p;
	bds_bufctl_t *bufctl;
	bds_uint i;
	bds_int colour_offset = cache_p->colour_offset;
	bds_int colour = cache_p->colour;
	slab_p = (bds_slab_desc_t)mgmt_mem;
	bufctl = (bds_bufctl_t*) BDS_BUFCTL(slab_p);
	for (i=0; i<cache_p->num; ++i)
		bufctl[i] = i+1;
	bds_spin_lock(&cache_p->cache_lock);
	slab_p->colour_offset = cache_p->colour_next;
	if (cache_p->colour) 
		cache_p->colour_next = 
			(cache_p->colour_next + colour_offset) % 
			(colour * colour_offset);
	bds_spin_unlock(&cache_p->cache_lock);
	slab_p->slab_magic = cache_p->slab_magic;
	slab_p->free = 0;
	slab_p->parent_cache = cache_p;
	slab_p->in_use = 0;
	bds_spin_init(&slab_p->slab_lock);
	if (OFF_SLAB(cache_p)) 
		slab_p->s_mem = (void*)((intptr_t)s_mem +
				slab_p->colour_offset);
	else 
		slab_p->s_mem = (void *)((intptr_t)s_mem +
				slab_p->colour_offset + cache_p->mgmt_size);
	
	/* Add the newly created slab to the partial list __NOT__ the free list
	   as we allocate a new slab only when we donot have memory to allocate 
	   from. So escape a few cycles here by directly adding to the partial
	   list */
	bds_mutex_lock(&cache_p->slab_list.list_lock);
	bds_list_add (&slab_p->next, &(cache_p->slab_list.slabs_partial));
	bds_mutex_unlock(&cache_p->slab_list.list_lock);
	/* Call the constructors if available */
	if (cache_p->ctor) {
		for(i=0; i<cache_p->num; ++i)
			cache_p->ctor(slab_p->s_mem + i*cache_p->buffer_size);
	}
	/* All slab opertations done. Slabs ready to use; Return */
	return;
}
	
/**
 * Increase the size of the cache by one slab
 * @cache_p: 	Pointer to the slab descriptor cache
 */
bds_int bds_cache_grow (bds_cache_desc_t cache_p)
{
	void *s_mem = NULL, *mgmt_mem = NULL;
	/* Get memory for the slab */
	s_mem = __get_free_pages(1 << cache_p->page_order);
	if (!s_mem) {
		return -ENOMEM;
	}
	if (cache_p->flags & CFLGS_OFF_SLAB) {
		mgmt_mem = bds_alloc(cache_p->mgmt_size);
		if (!mgmt_mem) {
			__reclaim_free_pages(s_mem, 1 << cache_p->page_order);
			return -ENOMEM;
		}
	} else {
		mgmt_mem = s_mem;
	}
	__setup_slab_desc(cache_p, mgmt_mem, s_mem);
	return 0;
}
void* bds_cache_alloc (bds_cache_desc_t cache_p)
{
	bds_slab_desc_t slab_p = NULL;
	void *obj = NULL;
#ifdef BDS_CONFIG_SMP
	printf ("SMP alloc\n");
#endif
	if (likely(!list_empty(&cache_p->slab_list.slabs_partial))) {
		slab_p = container_of (cache_p->slab_list.slabs_partial.next,
				struct _slab_desc, next);
	}
	else if (unlikely(!list_empty(&cache_p->slab_list.slabs_free))) {
		slab_p = container_of (cache_p->slab_list.slabs_free.next,
				struct _slab_desc, next);
		/* Delete it from the free list and add it to the partial list */
		bds_mutex_lock(&cache_p->slab_list.list_lock);
		bds_list_del(&slab_p->next);
		bds_list_add (&slab_p->next, 
				&(cache_p->slab_list.slabs_partial));
		bds_mutex_unlock(&cache_p->slab_list.list_lock);
	} else {
		/* Time to grow the cache by one slab. Call bds_cache_grow() now */
		if (bds_cache_grow(cache_p) < 0)
			return NULL;
		else {
			slab_p = container_of(
					cache_p->slab_list.slabs_partial.next,
					struct _slab_desc, next);
		}
	}

	/* By this time we have a valid slab_p in our hands. Try to allocate an
	   object from it */
	bds_spin_lock(&slab_p->slab_lock);
	obj = slab_p->s_mem + (slab_p->free * cache_p->buffer_size);
	bds_bufctl_t *bufctl = (bds_bufctl_t *)BDS_BUFCTL(slab_p);
	slab_p->free = bufctl[slab_p->free];
	bds_spin_unlock(&slab_p->slab_lock);

	/* After the last free object is allocated from the cache move the 
	   list to the full list */ 
	if (slab_p->free == cache_p->num) {
		bds_mutex_lock(&cache_p->slab_list.list_lock);
		bds_list_del(&slab_p->next);
		bds_list_add (&slab_p->next, &(cache_p->slab_list.slabs_full));
		bds_mutex_unlock(&cache_p->slab_list.list_lock);
	}
	return obj;
}


void bds_cache_free (bds_cache_desc_t cache_p, void *obj)
{
	bds_bufctl_t *bufctl;
	bds_slab_desc_t slab_p;
	size_t offset;
	if (OFF_SLAB(cache_p)) {
		/* Worst possible implementation : Find the 
		 slab which it belongs to. Check the partial list 
		 fist , then the full list*/
		if (__find_object_in_slab_list(
			&cache_p->slab_list.slabs_partial, 
					obj, &slab_p) == BDS_FOUND)
			goto found;
		if (__find_object_in_slab_list(&cache_p->slab_list.slabs_full, 
					obj, &slab_p) == BDS_FOUND)
			goto found;
		goto error;
	}
	else {
		size_t page_size = getpagesize();
		intptr_t mask = (~0 << __getpageshift());
		intptr_t iter;
		/* Locate the slab descriptor in the slab by moving backwards.
		   For single page slabs its O(1) here. For others is O(pagesize)
		 */
		for (iter = ((intptr_t)obj & mask); 
				*((bds_uint*)(iter)) != cache_p->slab_magic && 
				iter > ((intptr_t)obj - page_size); 
				iter -= page_size); 
		slab_p = (bds_slab_desc_t)iter;
	}
found:
	/* Find the offset of the object */
	offset = ((intptr_t)obj - (intptr_t)slab_p->s_mem)/cache_p->buffer_size;
	bufctl = (bds_bufctl_t*) BDS_BUFCTL(slab_p);
	bufctl[offset] = slab_p->free;
	slab_p->free = offset;
	return;
error:
	bds_error ("Object not allocated by bds alloc functions. Cannot free\n");	

}

/* Allocate @size from general caches. Return malloc if fail */
void* bds_alloc(size_t size) 
{
	bds_cache_desc_t cache_p;
	cache_p = __find_general_cache(size);
	if (unlikely(!cache_p)) 
		return malloc(size);
	return bds_cache_alloc(cache_p);
}

void* bds_alloc_chunk(bds_cache_desc_t cache_p, bds_int *nr_objs)
{
	bds_slab_desc_t slab_p;
	/* Find whether there is a free slab in the slabs_free list 
	   If no allocate a slab and send it back. Else allocate
	   from the free list and send back
	 */
	if (list_empty(cache_p->slab_list.slabs_partial.next))
	{
		/* No free slabs. Allocate one and give it */
		if (bds_cache_grow(cache_p) < 0)
			return NULL;
		else 
			slab_p = container_of(
					cache_p->slab_list.slabs_partial.next,
					struct _slab_desc, next);
	} else 
		slab_p = container_of(cache_p->slab_list.slabs_free.next,
				struct _slab_desc, next);
	/* Move the slab to the full list at once */
	bds_list_del(&slab_p->next);
	bds_list_add (&slab_p->next, &(cache_p->slab_list.slabs_full));
	*nr_objs = cache_p->num;
	return slab_p->s_mem;
}

void bds_free_chunk(bds_cache_desc_t cache_p, void *first_object)
{
	bds_cache_free(cache_p,first_object);
	return; 
}

#if 0
void bds_cache_desc_dump(bds_cache_desc_t cache_p) 
{
	if (!cache_p)
		return;
	bds_info ("*********************************\n");
	bds_info ("Page size: %d\n",getpagesize());
	bds_info ("Name: %s\n",cache_p->name);
	bds_info ("Flags: %#x\n",cache_p->flags);
	bds_info ("Colour: %d\n",cache_p->colour);
	bds_info ("Colour Offset: %d\n",cache_p->colour_offset);
	bds_info ("Buffer size: %u\n",cache_p->buffer_size);
	bds_info ("Cache page order: %d\n",cache_p->page_order);
	bds_info ("Cache slab size: %d\n",cache_p->slab_size);
	bds_info ("Objects per slab: %d\n",cache_p->num);
	bds_info ("Cache Descriptor in cache_cache: %d\n", cache_cache.num);
	bds_info ("slab desc size: %lu\n", sizeof (struct _slab_desc));
	bds_info ("mgmt size: %d\n", cache_p->mgmt_size);
	bds_info ("Cache desc size: %d\n", sizeof(*cache_p));
	bds_info ("*********************************\n");
}

void bds_cache_chain_dump (void) 
{
	bds_list_head_t head = cache_chain.next;
	bds_cache_desc_t cache_p;
	/* Iterate over the list of the caches present in the system */
	for (head = cache_chain.next; head != &cache_chain; head = head->next) {
		cache_p = container_of (head, struct _cache_desc, next);
		if (cache_p) {
			bds_info ("Cache_p->name : %s\n", cache_p->name);
		}
	}
}
#endif
