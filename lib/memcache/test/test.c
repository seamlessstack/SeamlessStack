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

int
main(void)
{
	char *key = "alphabet";
	char *value = "abcdefghijklmnopqrstuvwxyz";
	char *repvalue = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	memcached_st *memc = NULL;
	char *config = "localhost";
	int ret = -1;
	char *ptr = NULL;
	int len = -1;

	memc = sstack_memcache_init(config, 1, NULL);
	if (NULL == memc) {
		fprintf(stderr, "%s: Failed to get memcached handle\n", __FUNCTION__);
		return -1;
	}
	fprintf(stdout, "%s: Connection succeeded \n", __FUNCTION__);
	// Store an object
	fprintf(stdout, "%s: Storing %s\n", __FUNCTION__, value);
	ret = sstack_memcache_store(memc, key, value, strlen(value)+1, NULL);
	if (ret != 0) {
		fprintf(stderr, "%s: Failed to store the object into memcached."
			" Return value = %d\n", __FUNCTION__, ret);
		return -1;
	}
	// Read back the value
	ptr = sstack_memcache_read_one(memc, key, strlen(key), (size_t *)&len, NULL);
	if (NULL == ptr) {
		fprintf(stderr, "%s: Failed to read back the value stored. \n",
			__FUNCTION__);
		return -1;
	}
	fprintf(stderr, "%s: Value length = %d value = %s\n",
		__FUNCTION__, len, ptr);

	// Replace the value stored for the key
	ret = sstack_memcache_replace(memc, key, repvalue, (size_t )strlen(repvalue)+1,
		 (time_t)0, (uint32_t)0, NULL);
	if (ret != 0) {
		fprintf(stderr, "%s: Failed to replace the value stored. \n",
			__FUNCTION__);
		return -1;
	}
	// Re-read and display
	ptr = sstack_memcache_read_one(memc, key, strlen(key), (size_t *) &len, NULL);
	if (NULL == ptr) {
		fprintf(stderr, "%s: Failed to read back the value stored. \n",
			__FUNCTION__);
		return -1;
	}
	fprintf(stderr, "%s: Value length = %d value = %s\n",
		__FUNCTION__, len, ptr);

	sstack_memcache_destroy(memc, NULL);

	return 0;
}


