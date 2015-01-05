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
