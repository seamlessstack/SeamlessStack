#include <stdio.h>
#include <sstack_cache_api.h>
#include <stdlib.h>
#include <string.h>
#include <sstack_cache.h>
#include <sstack_lrucache.h>
#include <red_black_tree.h>

int
main(void)
{
	char *key = "data_123456789098765432123456789";
	char *value = "Test the cache code and harden it";
	memcached_st *memc = NULL;
	char *config = "localhost";
	int	ret = -1;
	sstack_cache_t *cache_entry = NULL;
	char *data;

	memc = sstack_memcache_init(config, 1, NULL);
	if (memc == NULL) {
		printf("%s: %d\n", __FUNCTION__, __LINE__);
		return (-1);
	}	

	printf("%s: %d connection succeeded\n", __FUNCTION__, __LINE__);

	ret = cache_init(NULL);
	if (ret != 0) {
		printf("%s: %d\n", __FUNCTION__, __LINE__);
		return (-1);
	}

	printf("%s: %d cache init done\n", __FUNCTION__, __LINE__);
	
	cache_entry = sstack_allocate_cache_entry(NULL);
	if (cache_entry == NULL) {
		printf("%s: %d\n", __FUNCTION__, __LINE__);
		return (-1);
	}

	printf("%s: %d cache entry allocated\n", __FUNCTION__, __LINE__);

	cache_entry->memcache.mc = memc;
	strcpy((char *)cache_entry->hashkey, key);
	ret = sstack_cache_store(value, strlen(value), cache_entry, NULL);

	if (ret != 0) {
		printf("%s: %d\n", __FUNCTION__, __LINE__);
	    return (-1);
	}
	
	printf("%s: %d cache_store success\n", __FUNCTION__, __LINE__);

	data = sstack_cache_get(key, 1000, NULL);
	if (data == NULL) {
		printf("%s: %d\n", __FUNCTION__, __LINE__);
		return (-1);
	}

	printf("%s: %d cache_get %s\n", __FUNCTION__, __LINE__);
}
