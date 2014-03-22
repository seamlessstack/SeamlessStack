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

/* sstack log directory */
char sstack_log_directory[PATH_MAX];
struct handle_payload_params;
sfsd_t sfsd;
bds_cache_desc_t sfsd_global_cache_arr[MAX_CACHE_OFFSET];

struct cache_entry {
	char name[32];
	size_t size;
};

struct cache_entry caches[MAX_CACHE_OFFSET] = {
	{"payload-cache", sizeof(sstack_payload_t)},
	{"param-cache", sizeof (struct handle_payload_params)},
	{"inode-cache", sizeof(sstack_inode_t)},
	{"data-4k-cache", 4096},
	{"data-64k-cache", 65536},
};

static int32_t sfsd_create_caches(sfsd_t *sfsd, struct cache_entry *centry)
{
	int32_t i,j, ret;
	/* Allocate memory for the caches array */
	sfsd->caches = malloc(sizeof(bds_cache_desc_t) * MAX_CACHE_OFFSET);

	if (sfsd->caches == NULL) {
		sfs_log(sfsd->log_ctx,
			SFS_ERR, "Caches array allocation failed\n");
		return -ENOMEM;
	}
	for(i = 0; i < MAX_CACHE_OFFSET; ++i) {
		ret = bds_cache_create(centry[i].name, centry[i].size, 0, NULL,
				       NULL, &sfsd_global_cache_arr[i]);
		if (ret != 0) {
			sfs_log(sfsd->log_ctx, SFS_ERR,
				"Could not create cache for %s\n",
				centry[i].name);
			goto error;
		}
	}

	return 0;

error:
	sfs_log(sfsd->log_ctx, SFS_ERR, "%s(): Bailing out..\n", __FUNCTION__);
	// bds_cache_destroy is not implemented yet
#if 0
	for (j = i; j >= 0; j--)
		bds_cache_destroy(sfsd_global_cache_arr[j]);
#endif
	return -ENOMEM;
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
	ASSERT ((0 == register_signals(&sfsd)),
			"Signal regisration failed", 1, 1, 0);
	/* Initialize and connect to the DB. DONOT initialize DB
	 here, it would be done by SFS*/
	db = create_db();
	ASSERT((db != NULL), "DB create failed", 1, 1, 0);
	db_register(db, mongo_db_init, mongo_db_open, mongo_db_close,
		mongo_db_insert, mongo_db_remove, mongo_db_iterate, mongo_db_get,
		mongo_db_seekread, mongo_db_update, mongo_db_delete,
		mongo_db_cleanup, ctx);
	sfsd.db = db;
	if (sfsd.db->db_ops.db_init(sfsd.log_ctx) != 0) {
	ASSERT ((0 == register_signals(&sfsd)),
			"Signal regisration failed", 1, 1, 0);
	}

	ASSERT((0 == sfsd_create_caches(&sfsd, caches)),
			"Cache creation failed", 1, 1, 0);

	sfsd.caches = sfsd_global_cache_arr;
	/* Initialize transport */
	init_transport(&sfsd);

	/* Initialize thread pool */
	init_thread_pool(&sfsd);

	/* Initialize the chunk domain */
	sfsd.chunk = sfsd_chunk_domain_init(&sfsd, sfsd.log_ctx);

	ASSERT((sfsd.chunk != NULL),
			"Chunk domain initialization failed", 1, 1, 0);

	/* Daemonize */
	//daemon(0, 0);

	while(1);

	/* Control never returns */
	return 0;
}
