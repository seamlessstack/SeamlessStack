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

void *bds_garbage_collect(void *params);
/* Function to return page aligned memory.
 * @size: Amount of memory to be allocted. Rounded off to the
 * nearest page
 */
static void* __get_free_pages(size_t num_pages)
{
	void *ptr;
	size_t page_size = getpagesize();
	ptr = mmap (NULL, num_pages * page_size,
		PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if (ptr == MAP_FAILED) {
		bds_error ("No pages available\n");
		return NULL;
	}
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
		    ((cache_p->num - 1) * cache_p->buffer_size)) {
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
/* _cache_estimate: Estimate the number of
   objects per slab for a given slab size
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
	atomic_set(&cache_p->refcount, 0);
	atomic_set(&cache_p->free_slab_count, 0);
	atomic_set(&cache_p->total_slab_count, 0);
	/* Initialize bds slab list and its lock*/
	INIT_LIST_HEAD (&cache_p->slab_list.slabs_partial);
	INIT_LIST_HEAD (&cache_p->slab_list.slabs_full);
	INIT_LIST_HEAD (&cache_p->slab_list.slabs_free);
	bds_mutex_init(&cache_p->slab_list.partial_list_lock);
	bds_mutex_init(&cache_p->slab_list.full_list_lock);
	bds_mutex_init(&cache_p->slab_list.free_list_lock);
	return 0;
}
/* bds_cache_create: Creates a BDS cache.
 * @params:
 *	name: Name of the cache ( < 32 chars)
 *	size: Size of each object
 *	flags: Static flags
 *	ctor, dtor: The constructor and destructor functions
 */
bds_status_t bds_cache_create (const char *name, size_t size,
		bds_int flags, void(*ctor)(void *), void(*dtor)(void *),
			       bds_cache_desc_t *cache_pp)
{
	bds_cache_desc_t cache_p;
	bds_uint linger = 8;
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
	cache_p->linger = linger;
	strncpy (cache_p->name, name, BDS_CACHE_NAME_MAX);
	/* Cache created : Add to list, Hold the cache_chain_lock */
	bds_list_add (&cache_p->next, &cache_chain);
	bds_mutex_unlock(&cache_chain_lock);
	*cache_pp = cache_p;
	return 0;
}

/* List locks must be held during calling this */
static void __destroy_slab_list(bds_cache_desc_t cache_p, bds_list_head_t list)
{
	void *fmem;
	bds_slab_desc_t slab_p;
	while (!list_empty(list)) {
		slab_p = container_of(list->next, struct _slab_desc, next);
		/* Remove it from the list */
		bds_list_del(&slab_p->next);
		/* Free the associated memory */
		if (OFF_SLAB(cache_p))
			fmem = (void *)((intptr_t)slab_p->s_mem -
					slab_p->colour_offset);
		else
			fmem = (void *)((intptr_t)slab_p->s_mem -
					slab_p->colour_offset -
					cache_p->mgmt_size);
		__reclaim_free_pages(fmem, 1 << cache_p->page_order);
		/* we have freed the data memory associated
		   to the slab. now its turn for the management */
		if (OFF_SLAB(cache_p)) {
			free(slab_p);
		}
	}

}

void bds_cache_destroy(bds_cache_desc_t cache_p, bds_int force)
{
	bds_list_head_t head, list;
	bds_slab_desc_t slab_p;
	bds_int free_free = 0, partial_free = 0, full_free = 0;
	if ((cache_p == NULL) ||
	    (atomic_read(&cache_p->refcount) > 0 && force == 0))
		return;

	/* Real free starts here.. First the free list, then the partial
	   and then the full list , order is immaterial here */
	if (bds_mutex_lock(&cache_p->slab_list.free_list_lock) == 0) {
		__destroy_slab_list(cache_p, &cache_p->slab_list.slabs_free);
		bds_mutex_unlock(&cache_p->slab_list.free_list_lock);
	}

	/* Now the partial list */
	if (bds_mutex_lock(&cache_p->slab_list.partial_list_lock) == 0) {
		__destroy_slab_list(cache_p, &cache_p->slab_list.slabs_partial);
		bds_mutex_unlock(&cache_p->slab_list.partial_list_lock);
	}

	/* Now the full list */
	list = &cache_p->slab_list.slabs_full;
	if (bds_mutex_lock(&cache_p->slab_list.full_list_lock) == 0) {
		__destroy_slab_list(cache_p, &cache_p->slab_list.slabs_full);
		bds_mutex_unlock(&cache_p->slab_list.full_list_lock);
	}

	/* By this time all the slabs are free. Free the cache_descriptor
	   altogether */
	bds_cache_free(&cache_cache, cache_p);

	return;
}
/**
 * bds_cache_init: Initialize the BDS slab allocator
 */

__attribute__((constructor))
void bds_cache_init (void)
{
	bds_int order, mgmt_size;
	pthread_attr_t attr;
	pthread_t thread;
	bds_int ret = -1;
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
	atomic_set(&cache_cache.refcount, 0);
	INIT_LIST_HEAD (&cache_cache.slab_list.slabs_partial);
	INIT_LIST_HEAD (&cache_cache.slab_list.slabs_full);
	INIT_LIST_HEAD (&cache_cache.slab_list.slabs_free);
	bds_mutex_init(&cache_cache.slab_list.partial_list_lock);
	bds_mutex_init(&cache_cache.slab_list.free_list_lock);
	bds_mutex_init(&cache_cache.slab_list.full_list_lock);
	bds_list_add (&cache_cache.next, &cache_chain);
	/* Create the garbage collector thread */
	pthread_attr_init(&attr);
	ret = pthread_create(&thread, &attr, bds_garbage_collect, NULL);
	if (ret == 0) {
		printf ("Garbage collector thread created\n");
	} else {
		perror("thread creation failed");
	}
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
	atomic_set(&slab_p->refcount, 0);
	bds_mutex_init(&slab_p->slab_lock);
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
	bds_mutex_lock(&cache_p->slab_list.partial_list_lock);
	bds_list_add (&slab_p->next, &(cache_p->slab_list.slabs_partial));
	bds_mutex_unlock(&cache_p->slab_list.partial_list_lock);
	atomic_inc(&cache_p->total_slab_count);
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
		mgmt_mem = malloc(cache_p->mgmt_size);
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

static inline void* __allocate_one(bds_cache_desc_t cache_p,
				   bds_slab_desc_t slab_p)
{
	void *obj;
	bds_bufctl_t *bufctl;
	if (slab_p == NULL)
		return NULL;
	/* We have a valid slab_p in our hands. Try to allocate an
	   object from it */

}
void* bds_cache_alloc (bds_cache_desc_t cache_p)
{
	bds_slab_desc_t slab_p = NULL;
	void *obj = NULL;
	bds_int status = -1;
	bds_bufctl_t *bufctl;
	/* Take the head of the partial list for allocation. If
	   partial list is empty try the head of the free list. If
	   that too is empty grow the cache by *linger* number of
	   slabs
	*/
	if (likely(!list_empty(&cache_p->slab_list.slabs_partial))) {
		slab_p = container_of(
			cache_p->slab_list.slabs_partial.next,
			struct _slab_desc, next);
		/* Get the slab lock, we might allocate from it */
		bds_mutex_lock(&slab_p->slab_lock);
	} else if (likely(!list_empty(&cache_p->slab_list.slabs_free))) {
		/* Remove the head of free list to head of partial list */
		if (bds_mutex_lock(&cache_p->slab_list.free_list_lock) == 0) {
			bds_list_del(&slab_p->next);
			bds_mutex_unlock(&cache_p->slab_list.free_list_lock);
		}
		/* Move the slab to the head of the partial list */
		if (bds_mutex_lock(&cache_p->slab_list.partial_list_lock) == 0) {
			bds_list_add(&slab_p->next, &(cache_p->slab_list.slabs_partial));
			bds_mutex_unlock(&cache_p->slab_list.partial_list_lock);
		}
		/* Take the head of the free list for allocation */
		slab_p = container_of(
			cache_p->slab_list.slabs_partial.next,
			struct _slab_desc, next);

		/* Get the lock, we might allocate from it */
		bds_mutex_lock(&slab_p->slab_lock);
	} else {
		status = bds_cache_grow(cache_p);
		if (status < 0) {
			obj = NULL;
		} else {
			return bds_cache_alloc(cache_p);
		}
	}

	/* By this time we should have a *valid* and **locked** slab_p
	   in our hands to allocate from or we have already exited the
	   function by returning NULL. So we go ahead with the allocation
	   if there is an empty object in it - the check for the emptyness
	   is required because while we might have been waiting for the
	   slab_lock, some other thread which already holds the lock
	   could have allocated the last object in the slab
	*/
	if (atomic_read(&slab_p->refcount) < cache_p->num) {
		obj = slab_p->s_mem + (slab_p->free * cache_p->buffer_size);
		bufctl = (bds_bufctl_t *)BDS_BUFCTL(slab_p);
		slab_p->free = bufctl[slab_p->free];
		atomic_inc(&slab_p->refcount);
		atomic_inc(&cache_p->refcount);
		if (atomic_read(&slab_p->refcount) == cache_p->num) {
			/* The slab is full now, this needs to be
			   moved to the full list. This can only happen
			   when the slab is in partial list
			*/
			if (bds_mutex_lock(&cache_p->slab_list.partial_list_lock) == 0) {
				bds_list_del(&slab_p->next);
				bds_mutex_unlock(&cache_p->slab_list.partial_list_lock);
			}
			if (bds_mutex_lock(&cache_p->slab_list.full_list_lock) == 0) {
				bds_list_add(&slab_p->next, &cache_p->slab_list.slabs_full);
				bds_mutex_unlock(&cache_p->slab_list.full_list_lock);
			}
		}
		printf ("Moved slab -> %p from partial to full\n", slab_p);
		bds_mutex_unlock(&slab_p->slab_lock);
	} else {
		bds_mutex_unlock(&slab_p->slab_lock);
		obj = bds_cache_alloc(cache_p);
	}
	return obj;
}

void __free_one(bds_cache_desc_t cache_p, bds_slab_desc_t slab_p, void *obj)
{
	bds_bufctl_t *bufctl;
	size_t offset;
	bds_uint value;
	/* Find the offset of the object */
	offset = ((intptr_t)obj -
		  (intptr_t)slab_p->s_mem)/cache_p->buffer_size;
	bufctl = (bds_bufctl_t*) BDS_BUFCTL(slab_p);
	bds_mutex_lock(&slab_p->slab_lock);
	bufctl[offset] = slab_p->free;
	slab_p->free = offset;
	atomic_dec(&cache_p->refcount);
	atomic_dec(&slab_p->refcount);
	/* List manipulation here -
	   1) if the slab is present in the partial list
	   and the object is the last object to be free, the
	   slab needs to move to the empty list.
	   2) if the slab is present in the full list and we
	   have freed the object, then the slab needs to move
	   to the partial list
	*/
	value = atomic_read(&slab_p->refcount);
	if (value == (cache_p->num - 1)) {
		/* The slab is in full list and needs to move to
		   partial list */
		if (bds_mutex_lock(&cache_p->slab_list.full_list_lock) == 0) {
			bds_list_del(&slab_p->next);
			bds_mutex_unlock(&cache_p->slab_list.full_list_lock);
		}
		if (bds_mutex_lock(&cache_p->slab_list.partial_list_lock)
		    == 0) {
			bds_list_add(&slab_p->next,
				     &cache_p->slab_list.slabs_partial);
			bds_mutex_unlock(&cache_p->slab_list.partial_list_lock);
		}
		printf ("freeing - Moved slab -> %p from full to partial\n", slab_p);
	}
	if (value == 0) {
		/* The slab is present in partial list and
		   needs to be moved to free list */
		if (bds_mutex_lock(&cache_p->slab_list.partial_list_lock)
		    == 0) {
			bds_list_del(&slab_p->next);
			bds_mutex_unlock(&cache_p->slab_list.partial_list_lock);
		}
		if (bds_mutex_lock(&cache_p->slab_list.free_list_lock) == 0) {
			bds_list_add(&slab_p->next,
				     &cache_p->slab_list.slabs_free);
			bds_mutex_unlock(&cache_p->slab_list.free_list_lock);
		}
		atomic_inc(&cache_p->free_slab_count);
		printf ("freeing - Moved slab -> %p from partial to free\n", slab_p);
	}
	bds_mutex_unlock(&slab_p->slab_lock);
	return;
}
void bds_cache_free (bds_cache_desc_t cache_p, void *obj)
{
	bds_bufctl_t *bufctl;
	bds_slab_desc_t slab_p;
	bds_int found = 0;
	bds_list_head_t list;
	if (OFF_SLAB(cache_p)) {
		if (__find_object_in_slab_list(
			    &cache_p->slab_list.slabs_partial,
			    obj, &slab_p) == BDS_FOUND) {
			found = 1;
		}
		if (found == 0) {
			if (__find_object_in_slab_list(
				    &cache_p->slab_list.slabs_full,
				    obj, &slab_p) == BDS_FOUND) {
				found = 1;
				printf ("Found object in full list\n");
			}
		}
	}
	else {
		size_t page_size = getpagesize();
		intptr_t mask = (~0 << __getpageshift());
		intptr_t iter;
		/* Locate the slab descriptor in the slab by moving backwards.
		   For single page slabs its O(1) here.
		   For others is O(pagesize)
		 */
		for (iter = ((intptr_t)obj & mask);
				*((bds_uint*)(iter)) != cache_p->slab_magic &&
				iter > ((intptr_t)obj - page_size);
				iter -= page_size);
		slab_p = (bds_slab_desc_t)iter;
		found = 1;
	}
	if (found == 1) {
		__free_one(cache_p, slab_p, obj);
	}
	printf ("Total slab count: %d, free slab count: %d\n", atomic_read(&cache_p->total_slab_count), atomic_read(&cache_p->free_slab_count));
	return;
}

void __destroy_free_slabs(bds_cache_desc_t cache_p, bds_uint gc_threshold)
{
	bds_slab_desc_t slab_p;
	bds_list_head_t node, head;
	bds_int i;
	void *fmem;
	bds_uint total_slabs, free_slabs, free_percentage;
	if (bds_mutex_lock(&cache_p->slab_list.free_list_lock) == 0) {
		while (1) {
			total_slabs = atomic_read(&cache_p->total_slab_count);
			free_slabs = atomic_read(&cache_p->free_slab_count);
			free_percentage = (free_slabs * 100) / total_slabs;
			if ((free_percentage < gc_threshold) ||
			    free_slabs < cache_p->linger)
				break;
			slab_p = container_of(
				cache_p->slab_list.slabs_free.next,
				struct _slab_desc, next);
			bds_list_del(&slab_p->next);
			/* Free the associated memory */
			if (OFF_SLAB(cache_p))
				fmem = (void *)((intptr_t)slab_p->s_mem -
						slab_p->colour_offset);
			else
				fmem = (void *)((intptr_t)slab_p->s_mem -
						slab_p->colour_offset -
						cache_p->mgmt_size);
			__reclaim_free_pages(fmem, 1 << cache_p->page_order);
			atomic_dec(&cache_p->total_slab_count);
			atomic_dec(&cache_p->free_slab_count);
			/* we have freed the data memory associated
			   to the slab. now its turn for the management */
			if (OFF_SLAB(cache_p)) {
				free(slab_p);
			}
		}
		bds_mutex_unlock(&cache_p->slab_list.free_list_lock);
	}
	return;
}

/* This function runs in the context of garbage
   collection thread. Jobs performed -
   1) Walk through the cache_chain
   2) Find free slabs percentage to total slab percentage
   3) Reclaim memory pages to release memory
*/
void *bds_garbage_collect(void *params)
{
	bds_uint gc_threshold;
	bds_cache_desc_t cache_p;
	bds_uint total = 0, free = 0, slabs_to_free = 0;
	/* By default allow max 25% of free slabs in the cache */
	if (params == NULL || (gc_threshold = *((bds_uint *)(params))) > 99)
		gc_threshold = 25;
	while(1) {
		if (bds_mutex_lock(&cache_chain_lock) == 0) {
			list_for_each_entry(cache_p, &cache_chain,  next) {
				total = atomic_read(&cache_p->total_slab_count);
				free = atomic_read(&cache_p->free_slab_count);
				if (free > ((gc_threshold * total) /100)) {
					slabs_to_free = free - 1 -
						((gc_threshold * total)/100);
				__destroy_free_slabs(cache_p, gc_threshold);
				}
			}
			bds_mutex_unlock(&cache_chain_lock);
		}
		sleep(10);
	}
	return NULL;
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

#if 1
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
