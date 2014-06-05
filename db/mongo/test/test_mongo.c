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

#include <stdio.h>
#include <fcntl.h>
char sstack_log_directory[] = "/tmp";
#include <mongo_db.h>

int main(void)
{
	log_ctx_t *ctx = NULL;
	int ret = -1;
	char *data;
	char sstack_log_directory[] = "/tmp";

	ctx = sfs_create_log_ctx();
	if (NULL == ctx) {
		printf("%s: Log ctx creation failed. Logging disabled\n",
				__FUNCTION__);
	} else {
		ret = sfs_log_init(ctx, SFS_DEBUG, "mongo_test");
		if (ret != 0)
			printf("%s: Log initialization failed. Log disabled\n",
					__FUNCTION__);
	}

	ret = mongo_db_init(ctx);
	if (ret != 0) {
		printf("%s: DB initialization failed. Bailing out\n",
				__FUNCTION__);

		return -1;
	}

	// Insert a record
	ret = mongo_db_insert("hello", "world", strlen("world"), INODE_TYPE, ctx);
	if (ret != 1) {
		printf("%s: Unable to insert record into DB. Bailing out \n",
				__FUNCTION__);

		return -1;
	}

	// Update a record
	ret = mongo_db_update("hello", "WoRlD!", strlen("WoRlD!"), INODE_TYPE, ctx);
	if (!ret) {
		printf("%s: Unable to update record in DB. Bailing out \n",
				__FUNCTION__);

		return -1;
	}

	// Query the record
	ret = mongo_db_get("hello", (char **) &data, INODE_TYPE, ctx);
	if (!ret) {
		printf("%s: Unable to query the record. Bailing out \n",
				__FUNCTION__);

		return -1;
	}

	// Insert another record
	ret = mongo_db_insert("hell", "with the world", strlen("with the world"),
			INODE_TYPE, ctx);
	if (!ret) {
		printf("%s: Unable to insert record into DB. Bailing out \n",
				__FUNCTION__);

		return -1;
	}

	// Query the first record
	ret = mongo_db_get("hello", (char **) &data, INODE_TYPE, ctx);
	if (!ret) {
		printf("%s: Unable to query the record. Bailing out \n",
				__FUNCTION__);

		return -1;
	}

	// Query the second record
	ret = mongo_db_get("hell", (char **) &data, INODE_TYPE, ctx);
	if (!ret) {
		printf("%s: Unable to query the record. Bailing out \n",
				__FUNCTION__);

		return -1;
	}

	ret = mongo_db_cleanup(ctx);

	return 0;
}
