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

#include <stdio.h>
#include <sstack_cache_api.h>
#include <stdlib.h>
#include <string.h>
#include <sstack_cache.h>
#include <sstack_lrucache.h>
#include <red_black_tree.h>

char *value1 = "qwewretrt  tprtritor r trotrtifsldfsfkdlsfkslfsfffkds n,dfnsnf,sddfs fdlkfdkfhsfhwe ejfsjflsjdfksfskdfj  efljdlfjslkdjfsjdkfjsljfsj sjfsjdflslfjsljfldsjsjlfjsl jljfjsljf lsjfljsdlfjsjl jlajljdl jlsjflsjl jalfjksdj ljsfljsldkjflj ljsfl js";

int
main(void)
{
	char *key = "data_123456789098765432123456789";
	char *key1 = "data_123456789098765432123456798";
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

	printf("%s: %d cache_get %s\n", __FUNCTION__, __LINE__, data);

	cache_entry = sstack_allocate_cache_entry(NULL);
	if (cache_entry == NULL) {
		printf("%s: %d\n", __FUNCTION__, __LINE__);
		return (-1);
	}

	printf("%s: %d cache entry allocated\n", __FUNCTION__, __LINE__);

	cache_entry->memcache.mc = memc;
	strcpy((char *)cache_entry->hashkey, key1);
	ret = sstack_cache_store(value1, strlen(value1), cache_entry, NULL);

	if (ret != 0) {
		printf("%s: %d\n", __FUNCTION__, __LINE__);
	    return (-1);
	}

	printf("%s: %d cache_store success\n", __FUNCTION__, __LINE__);

	data = sstack_cache_get(key1, 1000, NULL);
	if (data == NULL) {
		printf("%s: %d\n", __FUNCTION__, __LINE__);
		return (-1);
	}

	printf("%s: %d cache_get %s\n", __FUNCTION__, __LINE__, data);

	data = sstack_cache_get(key, 1000, NULL);
	if (data == NULL) {
		printf("%s: %d\n", __FUNCTION__, __LINE__);
		return (-1);
	}

	printf("%s: %d cache_get %s\n", __FUNCTION__, __LINE__, data);

	ret = sstack_cache_purge(key, NULL);
	if (ret != 0) {
		printf("%s: %d\n", __FUNCTION__, __LINE__);
		return (-1);
	}

	data = sstack_cache_get(key, 1000, NULL);
	if (data == NULL) {
		printf("%s: %d\n", __FUNCTION__, __LINE__);
		return (-1);
	}
}
