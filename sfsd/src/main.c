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

#include <stdint.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sstack_log.h>
#include <sstack_transport.h>
#include <sstack_sfsd.h>
#include <bds_slab.h>
#include <sstack_chunk.h>
#include <sstack_db.h>
#include <sstack_md.h>
#include <mongo_db.h>
#include <sstack_serdes.h>

/* sstack log directory */
char sstack_log_directory[PATH_MAX];
struct handle_payload_params;
sfsd_t sfsd;
bds_cache_desc_t *sfsd_global_cache_arr = NULL;
log_ctx_t *gbl_ctx = NULL;
struct cache_entry {
	char name[32];
	size_t size;
};

struct cache_entry caches[] = {
	{"param-cache", sizeof (struct handle_payload_params)},
	{"inode-cache", sizeof(sstack_inode_t)},
};

static int32_t sfsd_create_caches(sfsd_t *sfsd)
{
	int32_t i,j, ret;
	/* Allocate memory for the caches array */
	sfsd->local_caches = malloc(sizeof(bds_cache_desc_t)
								* (MAX_CACHE_OFFSET+1));

	if (sfsd->local_caches == NULL) {
		sfs_log(sfsd->log_ctx,
			SFS_ERR, "Caches array allocation failed\n");
		return -ENOMEM;
	}
	ret = bds_cache_create("param-cache",
						   sizeof(struct handle_payload_params),
						   0, NULL, NULL,
						   &sfsd->local_caches[HANDLE_PARAM_OFFSET]);
	if (ret != 0) {
		sfs_log(sfsd->log_ctx, SFS_CRIT, "param-cache create failed\n");
		return -ENOMEM;
	}
	sfs_log(sfsd->log_ctx, SFS_DEBUG, "param-cache created\n");

	ret = bds_cache_create("inode-cache",
						   sizeof(sstack_inode_t),
						   0, NULL, NULL,
						   &sfsd->local_caches[INODE_CACHE_OFFSET]);
	if (ret != 0) {
		bds_cache_destroy(sfsd->local_caches[HANDLE_PARAM_OFFSET], 0);
		sfs_log(sfsd->log_ctx, SFS_CRIT, "inode-cache create failed\n");
	}
	sfs_log(sfsd->log_ctx, SFS_DEBUG, "inode-cache created\n");
	sfsd_global_cache_arr = sfsd->local_caches;

	return 0;

}

int main(int argc, char **argv)
{

	log_ctx_t *ctx = NULL;
	int log_ret = 0;
	db_t *db;

	/* Set up the log directory */
	if (argc == 1) {
		fprintf (stdout, "Usage: %s <log directory> <sfs address>\n",
				argv[0]);
	} else {
		strncpy(sstack_log_directory, argv[1], PATH_MAX);
	}
	memset(&sfsd, 0, sizeof(sfsd));

	/* Get the server address also from the command line */
	strcpy(sfsd.sfs_addr, argv[2]);

	/* initialize logging */
	ctx = sfs_create_log_ctx();
	ASSERT((NULL != ctx), "Log create failed", 1, 1, 0);
	log_ret = sfs_log_init(ctx, SFS_DEBUG, "sfsd");
	ASSERT((0 == log_ret), "Log init failed", 1, 1, 0);

	sfsd.log_ctx = ctx;

	/* Register signals */
	if (register_signals(&sfsd) != 0) {
		sfs_log(sfsd.log_ctx, SFS_CRIT, "Signal regisration failed");
		return -EINVAL;
	}
	/* Initialize and connect to the DB. DONOT initialize DB
	 here, it would be done by SFS*/
	db = create_db();
	ASSERT((db != NULL), "DB create failed", 1, 1, 0);
	db_register(db, mongo_db_init, mongo_db_open, mongo_db_close,
		mongo_db_insert, mongo_db_remove, mongo_db_iterate, mongo_db_get,
		mongo_db_seekread, mongo_db_update, mongo_db_delete,
		mongo_db_cleanup, ctx);
	sfsd.db = db;

	if (sfsd.db->db_ops.db_open(sfsd.log_ctx) != 0) {
		sfs_log(sfsd.log_ctx, SFS_CRIT, "%s() - db open failed\n");
		return -EIO;
	}

	if (sfsd_create_caches(&sfsd)) {
		sfs_log(sfsd.log_ctx, SFS_CRIT, "Caches creation failed");
		return -ENOMEM;
	}

	/* Fill up the global variable to be used */
	sfsd_global_cache_arr = sfsd.local_caches;
	/* Initialize transport */
	init_transport(&sfsd);

	/* Initialize serdes library */
	if (sstack_serdes_init(&sfsd.serdes_caches) != 0) {
		sfs_log(sfsd.log_ctx, SFS_CRIT, "SERDES init failed");
		return -ENOMEM;
	};

	if (sstack_helper_init(sfsd.log_ctx) != 0) {
		sfs_log(sfsd.log_ctx, SFS_CRIT, "helper init failed\n");
		return -ENOMEM;
	}

	/* Initialize thread pool */
	init_thread_pool(&sfsd);

	/* Initialize the chunk domain */
	sfsd.chunk = sfsd_chunk_domain_init(&sfsd, sfsd.log_ctx);

	ASSERT((sfsd.chunk != NULL),
			"Chunk domain initialization failed", 1, 1, 0);

	/* Daemonize */
	// daemon(0, 0);
	gbl_ctx = sfsd.log_ctx;
	while(1) {
		sleep(100);
	}


	/* Control never returns */
	return 0;
}
