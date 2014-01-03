/*************************************************************************
 * 
 * SEAMLESSSTACK CONFIDENTIAL
 * __________________________
 * 
 *  [2012] - [2013]  SeamlessStack Inc
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

	memc = sstack_cache_init(config, 1, NULL);
	if (NULL == memc) {
		fprintf(stderr, "%s: Failed to get memcached handle\n", __FUNCTION__);
		return -1;
	}
	fprintf(stdout, "%s: Connection succeeded \n", __FUNCTION__);
	// Store an object
	fprintf(stdout, "%s: Storing %s\n", __FUNCTION__, value);
	ret = sstack_cache_store(memc, key, value, strlen(value)+1, NULL);	
	if (ret != 0) {
		fprintf(stderr, "%s: Failed to store the object into memcached."
			" Return value = %d\n", __FUNCTION__, ret);
		return -1;
	}
	// Read back the value
	ptr = sstack_cache_read_one(memc, key, strlen(key), (size_t *)&len, NULL);
	if (NULL == ptr) {
		fprintf(stderr, "%s: Failed to read back the value stored. \n",
			__FUNCTION__);
		return -1;
	}
	fprintf(stderr, "%s: Value length = %d value = %s\n",
		__FUNCTION__, len, ptr);

	// Replace the value stored for the key
	ret = sstack_cache_replace(memc, key, repvalue, (size_t )strlen(repvalue)+1,
		 (time_t)0, (uint32_t)0, NULL);
	if (ret != 0) {
		fprintf(stderr, "%s: Failed to replace the value stored. \n",
			__FUNCTION__);
		return -1;
	}
	// Re-read and display
	ptr = sstack_cache_read_one(memc, key, strlen(key), (size_t *) &len, NULL);
	if (NULL == ptr) {
		fprintf(stderr, "%s: Failed to read back the value stored. \n",
			__FUNCTION__);
		return -1;
	}
	fprintf(stderr, "%s: Value length = %d value = %s\n",
		__FUNCTION__, len, ptr);
	
	sstack_cache_destroy(memc, NULL);

	return 0;
}
	
	
