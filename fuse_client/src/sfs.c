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

#define _XOPEN_SOURCE 500
#define _BSD_SOURCE 1
#include <fuse.h>
#include <fuse/fuse_opt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/vfs.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <inttypes.h>
#include <sys/xattr.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <dirent.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>


/* Local headers */
#include <sstack_jobs.h>
#include <sstack_md.h>
#include <sstack_log.h>
#include <sstack_helper.h>
#include <sstack_version.h>
#include <sstack_bitops.h>
#include <sstack_transport_tcp.h>
#include <sstack_serdes.h>
#include <policy.h>
#include <fused_internal.h>
#include <fused.h>
#include <fused_entry.h>
#include <sfs_job.h>
#include <sfs_jobid_tree.h>
#include <sfs_jobmap_tree.h>


/* Macros */
#define MAX_INODE_LEN 40 // Maximum len of uint64_t is 39
#define MAX_JOB_SUBMIT_RETRIES 10 // Maximum number of retries
#define MAX_HIGH_PRIO 64
#define MAX_MEDIUM_PRIO 16
#define MAX_LOW_PRIO 4
#define FUSED_JOB_SLEEP 5

/* BSS */
log_ctx_t *fused_ctx = NULL;
char sstack_log_directory[PATH_MAX] = { '\0' };
sfs_log_level_t sstack_log_level = SFS_DEBUG;
sfs_chunk_entry_t	*fused_chunks = NULL;
uint64_t nchunks = 0;
char fused_mountpoint[MOUNTPOINT_MAXPATH] = { '\0' };
unsigned long long active_inodes = 0;
sstack_client_handle_t fused_handle = 0;
sstack_thread_pool_t *fused_thread_pool = NULL;
sstack_transport_t transport;
jobmap_tree_t *jobmap_tree = NULL;
jobid_tree_t *jobid_tree = NULL;
pthread_spinlock_t jobmap_lock;
pthread_spinlock_t jobid_lock;
extern sstack_bitmap_t *fused_job_id_bitmap;
/*
 * jobs is the job list of unsubmitted (to sfsd) jobs
 * pending_jobs is the list of jobs waiting for completion
 */
sfs_job_queue_t *jobs = NULL;
sfs_job_queue_t *pending_jobs = NULL;


/* Structure definitions */

static struct fuse_opt fused_opts[] = {
	FUSE_OPT_KEY("-D=%s", KEY_LOG_FILE_DIR),
	FUSE_OPT_KEY("--help", KEY_HELP),
	FUSE_OPT_KEY("-h", KEY_HELP),
	FUSE_OPT_KEY("-L=%d", KEY_LOG_LEVEL),
	FUSE_OPT_KEY("--version", KEY_VERSION),
	FUSE_OPT_KEY("-V", KEY_VERSION),
	FUSE_OPT_END
};


/* Forward declarations */
static int fused_store_branch(char *branch);
static int fused_remove_branch(char *branch);


/* Functions */

static int
add_inodes(const char *path)
{
	sstack_extent_t extent;
	sstack_inode_t inode;
	struct stat status;
	int ret = -1;
	sstack_file_handle_t *ep = NULL;
	char inode_str[MAX_INODE_LEN] = { '\0' };

	sfs_log(fused_ctx, SFS_DEBUG, "%s: path = %s\n", __FUNCTION__, path);
	if (lstat(path, &status) == -1) {
		sfs_log(fused_ctx, SFS_ERR,
			"%s: stat failed on %s with error %d\n",
			__FUNCTION__, path, errno);
		return -errno;
	}


	// Populate inode structure
	inode.i_num = get_free_inode();
	strcpy(inode.i_name, path);
	inode.i_uid = status.st_uid;
	inode.i_gid = status.st_gid;
	inode.i_mode = status.st_mode;
	switch (status.st_mode & S_IFMT) {
		case S_IFDIR:
			inode.i_type = DIRECTORY;
			break;
		case S_IFREG:
			inode.i_type = REGFILE;
			break;
		case S_IFLNK:
			inode.i_type = SYMLINK;
			break;
		case S_IFBLK:
			inode.i_type = BLOCKDEV;
			break;
		case S_IFCHR:
			inode.i_type = CHARDEV;
			break;
		case S_IFIFO:
			inode.i_type = FIFO;
			break;
		case S_IFSOCK:
			inode.i_type = SOCKET_TYPE;
			break;
		default:
			inode.i_type = UNKNOWN;
			break;
	}

	// Make sure we are not looping
	// TBD

	memcpy(&inode.i_atime, &status.st_atime, sizeof(struct timespec));
	memcpy(&inode.i_ctime, &status.st_ctime, sizeof(struct timespec));
	memcpy(&inode.i_mtime, &status.st_mtime, sizeof(struct timespec));
	inode.i_size = status.st_size;
	inode.i_ondisksize = (status.st_blocks * 512);
	inode.i_numreplicas = 1; // For now, single copy
	// Since the file already exists, done't split it now. Split it when
	// next write arrives
	inode.i_numextents = 1;
	inode.i_numerasure = 0; // No erasure code segments
	inode.i_xattrlen = 0; // No extended attributes
	inode.i_links = status.st_nlink;
	sfs_log(fused_ctx, SFS_INFO,
		"%s: nlinks for %s are %d\n", __FUNCTION__, path, inode.i_links);
#if 0
	// Populate size of each extent
	policy = get_policy(path);
	if (NULL == policy) {
		sfs_log(fused_ctx, SFS_INFO,
			"%s: No policies specified. Default policy applied\n",
			__FUNCTION__);
		inode.i_policy.pe_attr.a_qoslevel = QOS_LOW;
		inode.i_policy.pe_attr.a_ishidden = 0;
	} else {
		// Got the policy
		sfs_log(fused_ctx, SFS_INFO,
			"%s: Got policy for file %s\n", __FUNCTION__, path);
		sfs_log(fused_ctx, SFS_INFO, "%s: ver %s qoslevel %d hidden %d \n",
			__FUNCTION__, path, policy->pe_attr.ver,
			policy->pe_attr.a_qoslevel,
			policy->pe_attr.a_ishidden);
		memcpy(&inode.i_policy, policy, sizeof(policy_entry_t));
	}
#endif
	// Populate the extent
	memset((void *) &extent, 0, sizeof(sstack_extent_t));
	extent.e_sizeondisk = status.st_size;
	if (inode.i_type == REGFILE) {
		extent.e_cksum = sstack_checksum(fused_ctx, path); // CRC32
		sfs_log(fused_ctx, SFS_INFO, "%s: checksum of %s is %lu \n",
			__FUNCTION__, path, extent.e_cksum);
	} else
		extent.e_cksum = 0; // Place holder

	ep = malloc(sizeof(sstack_file_handle_t));
	if (NULL == ep) {
		sfs_log(fused_ctx, SFS_ERR, "%s: Allocation failed for e_path\n",
						__FUNCTION__);
		return -errno;
	}
	ep->name_len = strlen(path);
	strncpy((char *)&ep->name, path, ep->name_len);
	ep->proto = NFS;
	ep->address.protocol = IPV4;
	strncpy((char *) ep->address.ipv4_address, "127.0.0.1",
					strlen("127.0.0.1")+1);

	extent.e_path = ep;
	inode.i_extent = &extent;
	inode.i_erasure = NULL; // No erasure coding info for now
	inode.i_xattr = NULL; // No extended attributes carried over

	// Store inode
	ret = put_inode(&inode, db, 0);
	if (ret != 0) {
		sfs_log(fused_ctx, SFS_ERR, "%s: Failed to store inode #%lld for "
				"path %s. Error %d\n", __FUNCTION__, inode.i_num,
				inode.i_name, errno);
		return -errno;
	}

	free(ep);
	// Store inode <-> path into reverse lookup
	sprintf(inode_str, "%lld", inode.i_num);
	ret = sstack_memcache_store(mc, path, inode_str, (strlen(inode_str) + 1),
					fused_ctx);
	if (ret != 0) {
		sfs_log(fused_ctx, SFS_ERR, "%s: Unable to store object into memcached."
				" Key = %s value = %s \n", __FUNCTION__, path, inode_str);
		return 0;
	}

	active_inodes ++; // Called during initialization and in single thread.

	return 0;
}


static void
populate_db(const char *dir_name)
{
	DIR *d = NULL;

	/* Open the directory specified by "dir_name". */
	sfs_log(fused_ctx, SFS_ERR, "%s: dir_name = %s \n", __FUNCTION__,
			dir_name);

	d = opendir (dir_name);

	// Check it was opened
	if (! d) {
		sfs_log (fused_ctx,SFS_ERR,
				"%s: Cannot open directory '%s': %s\n",
				__FUNCTION__, dir_name, strerror (errno));
		exit (-1);
	}

	while (1) {
		struct dirent * entry;
		const char * d_name;
		char path[PATH_MAX];
		char *p = NULL;

		// "Readdir" gets subsequent entries from "d"
		entry = readdir (d);
		if (! entry) {
			/*
			 * There are no more entries in this directory, so break
			 * out of the while loop.
			 */
			break;
		}
		d_name = entry->d_name;
		// Don't bother for . and ..
		if (strcmp (d_name, "..") != 0 && strcmp (d_name, ".") != 0) {
			sprintf(path, "%s%s", dir_name, d_name);
			sfs_log(fused_ctx, SFS_ERR, "%s: dir_name = %s, d_name = %s \n",
				__FUNCTION__, dir_name, d_name);
			p = rep(path, '/');
			add_inodes(p);
			free(p);
		}

		// See if "entry" is a subdirectory of "d"
		if (entry->d_type & DT_DIR) {
			// Check that the directory is not "d" or d's parent
			if (strcmp (d_name, "..") != 0 &&
						strcmp (d_name, ".") != 0) {
				int path_length;
				char path[PATH_MAX];

				path_length = snprintf (path, PATH_MAX, "%s/%s",
						dir_name, d_name);
				if (path_length >= PATH_MAX) {
					sfs_log (fused_ctx, SFS_ERR,
						"%s: Path length has got too long.\n",
						__FUNCTION__);
					exit (-1);
				}
				// Recursively call populate_db with the new path
				populate_db (path);
			}
		}
	}
	// After going through all the entries, close the directory
	if (closedir (d)) {
		sfs_log (fused_ctx, SFS_ERR, "%s: Could not close '%s': %s\n",
			__FUNCTION__, dir_name, strerror (errno));
		exit (-1);
	}
}


static void * fused_handle_connection(void * arg);

static void
fused_accept_connection(void *arg)
{
	pthread_t recv_thread;
	pthread_attr_t recv_attr;
	struct sockaddr addr;
	socklen_t len;
	int accept_fd = -1;

	// Call server setup
	fused_handle = transport.transport_ops.server_setup(&transport);
	if (fused_handle == -1) {
		// Server socket creation failed.
		sfs_log(fused_ctx, SFS_ERR, "%s: Unable to create sfs socket. "
					"Error = %d \n", __FUNCTION__, errno);
		// cleanup
		db->db_ops.db_close(fused_ctx);
		sstack_transport_deregister(TCPIP, &transport);

		return NULL;
	}

	while(1) {
		int ret = -1;

		accept_fd = accept(fused_handle, &addr, &len);
		if (accept_fd == -1) {
			sfs_log(fused_ctx, SFS_ERR, "%s: accept failed with error %d \n",
					__FUNCTION__, errno);
			// cleanup
			sstack_transport_deregister(TCPIP, &transport);
			close(fused_handle);

			return NULL;
		}

		sfs_log(fused_ctx, SFS_DEBUG, "%s: accept_fd = %d transport = 0x%x \n",
				__FUNCTION__, accept_fd, &transport);

		sfs_log(fused_ctx, SFS_DEBUG, "%s: server setup complete \n",
				__FUNCTION__);
		// Create a thread to handle sfs<->sfsd communication
		if(pthread_attr_init(&recv_attr) == 0) {
			pthread_attr_setscope(&recv_attr, PTHREAD_SCOPE_SYSTEM);
			pthread_attr_setstacksize(&recv_attr, 131072); // 128KiB
			pthread_attr_setdetachstate(&recv_attr, PTHREAD_CREATE_DETACHED);
			ret = pthread_create(&recv_thread, &recv_attr,
					fused_handle_connection, (void *) accept_fd);
		}

		if (ret != 0) {
			sfs_log(fused_ctx, SFS_CRIT, "%s: Unable to create thread to "
					"handle sfs<->sfsd communication\n", __FUNCTION__);
			return NULL;
		}
		sfs_log(fused_ctx, SFS_DEBUG, "%s: sfs<->sfsd communication thread "
				"created \n", __FUNCTION__);
	}

}
/*
 * fused_handle_connction - Thread function to listen to requests from sfsds
 *
 * arg - argument (not used)
 *
 * Never returns unless an error. On error, returns NULL.
 */

static void *
fused_handle_connection(void * arg)
{
	int ret = -1;
	uint32_t mask = READ_BLOCK_MASK;
	sstack_payload_t *payload;
	int num_retries = 0;
	int accept_fd = (int) arg;

	sfs_log(fused_ctx, SFS_DEBUG, "%s: Started \n", __FUNCTION__);

	while (1) {
		// sfs_log(fused_ctx, SFS_DEBUG, "%s: Calling select \n", __FUNCTION__);
		if (NULL == transport.transport_ops.select) {
			sfs_log(fused_ctx, SFS_ERR, "%s: select op not set \n",
							__FUNCTION__);
			return NULL;
		}
		ret = transport.transport_ops.select(accept_fd, mask);
		if (ret != READ_NO_BLOCK) {
		//	sfs_log(fused_ctx, SFS_INFO, "%s: Connection down. Waiting for "
		//					"retry \n", __FUNCTION__);
			sleep(1);
			/* Its a non-blocking select call. So, it could come out of
			 * select even though there is nothing to read. So just go back
			 * to select
			 */
			continue;
		}

		// Receive a payload
		payload = sstack_recv_payload(accept_fd, &transport, fused_ctx);
		if (NULL == payload) {
			sfs_log(fused_ctx, SFS_ERR, "%s: Unable to receive payload. "
							"Error = %d\n", __FUNCTION__, errno);
			continue;
		}

		sfs_log(fused_ctx, SFS_INFO, "%s: Response received from sfsd\n",
				__FUNCTION__);

retry:
		sfs_log(fused_ctx, SFS_DEBUG, "%s: sfs_process_payload = 0x%"PRIx64" "
				"pool = %"PRIx64" \n",
				__FUNCTION__, sfs_process_payload, fused_thread_pool);

		ret = sstack_thread_pool_queue(fused_thread_pool, sfs_process_payload,
						(void *)payload);
		if (ret != 0) {
			sfs_log(fused_ctx, SFS_ERR, "%s: Queueing job failed. "
							"Error = %d \n", __FUNCTION__, errno);
			// Only ENOMEM is the possible errno
			// Wait for sometime and retry back
			sleep(2);
			num_retries ++;
			if (num_retries < MAX_JOB_SUBMIT_RETRIES)
				goto retry;
			else {
				sfs_log(fused_ctx, SFS_ERR, "%s: Failed to submit job to thread "
								"pool after %d retries. Quitting ...\n",
								__FUNCTION__, num_retries);
				return NULL;
			}
		}
		sfs_log(fused_ctx, SFS_INFO, "%s: Job successfully processed \n",
				__FUNCTION__);
	}

	// Ideally control should never reach here

	return NULL;
}

/*
 * fused_init_thread_pool - Create a thread pool to handle jobs
 *
 * Create thread pool with minimum 4 threads and maximum of number of
 * entries in listen queue (currently 1024).
 *
 * Returns 0 on success and -1 on failure.
 */

static int
fused_init_thread_pool(void)
{
	pthread_attr_t attr;

	// Create thread pool to handle sfsd requests/responses
	pthread_attr_init(&attr);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	// pthread_attr_setstacksize(&attr, 65536);
	// Create SSTACK_BACKLOG threads at max
	// Current value is 128
	fused_thread_pool = sstack_thread_pool_create(4, SSTACK_BACKLOG, 30, &attr);
	if (NULL == fused_thread_pool) {
		sfs_log(fused_ctx, SFS_ERR, "%s: Unable to create sfs threadpool. "
					"Connection handling is implausible. Error = %d \n ",
					__FUNCTION__, errno);
		return -1;
	}

	sfs_log(fused_ctx, SFS_INFO, "%s: FUSED thread pool successfully created \n",
						__FUNCTION__);

	return 0;
}



static void *
fused_init(struct fuse_conn_info *conn)
{
	pthread_t dispatcher_thread;
	pthread_attr_t dispatcher_attr;
	pthread_t mon_thread;
	pthread_attr_t mon_attr;
	char *intf_addr;
	int ret = -1;

	sfs_log(fused_ctx, SFS_DEBUG, "%s: Started \n", __FUNCTION__);

	// Create serdes cache
	ret = sstack_serdes_init(&serdes_caches);

	if (ret != 0) {
		sfs_log(fused_ctx, SFS_CRIT, "%s: SERDES cache create failed. \n",
				__FUNCTION__);
		return -ENOMEM;
	}

	ret = sstack_helper_init(fused_ctx);

	if (ret != 0) {
		sfs_log(fused_ctx, SFS_CRIT, "%s() - Helper init failed\n",
				__FUNCTION__);
		return -ENOMEM;
	}

	// Initialize TCP transport
	transport.transport_type = TCPIP;
	// get local ip address
	// eth0 is the assumed interface
	intf_addr = get_local_ip("eth0", IPv4, fused_ctx);
	if (NULL == intf_addr) {
		sfs_log(fused_ctx, SFS_ERR, "%s: Please enable eth0 interface "
						"and retry.\n", __FUNCTION__);
		return NULL;
	}
	sfs_log(fused_ctx, SFS_DEBUG, "%s: interface addr = %s \n",
					__FUNCTION__, intf_addr);
	transport.transport_hdr.tcp.ipv4 = 1;
	strcpy((char *) &transport.transport_hdr.tcp.ipv4_addr, intf_addr);
	transport.transport_hdr.tcp.port = htons(atoi(FUSED_SERVER_PORT));
	transport.transport_ops.rx = tcp_rx;
	transport.transport_ops.tx = tcp_tx;
	transport.transport_ops.client_init = tcp_client_init;
	transport.transport_ops.server_setup = tcp_server_setup;
	transport.transport_ops.select = tcp_select;
	transport.ctx = fused_ctx;
	ret = sstack_transport_register(TCPIP, &transport, transport.transport_ops);
	sfs_log(fused_ctx, SFS_DEBUG, "%s: transport regietered \n", __FUNCTION__);
	// Create a thread to wait for monitor
	if(pthread_attr_init(&mon_attr) == 0) {
		pthread_attr_setscope(&mon_attr, PTHREAD_SCOPE_SYSTEM);
		pthread_attr_setstacksize(&mon_attr, 131072); // 128KiB
		pthread_attr_setdetachstate(&mon_attr, PTHREAD_CREATE_DETACHED);
		ret = pthread_create(&mon_thread, &mon_attr,
						mon_htbt, NULL);
	}
	if (ret != 0) {
		sfs_log(fused_ctx, SFS_CRIT, "%s: Unable to create thread to "
						"handle  communed<->mon communication\n", __FUNCTION__);
		return NULL;
	}
	sfs_log(fused_ctx, SFS_DEBUG, "%s: fused<->mon communication thread "
					"created \n", __FUNCTION__);

	ret = fused_init_thread_pool();
	if (ret != 0) {
		// Thread pool creation failed.
		// Exit out as we can't handle requests anyway
		sfs_log(fused_ctx, SFS_ERR, "%s: Failed to create thread pool. "
						"Exiting ...\n", __FUNCTION__);
		// pthread_kill(recv_thread, SIGKILL);
		sstack_transport_deregister(TCPIP, &transport);

		return NULL;
	}

	sfs_log(fused_ctx, SFS_DEBUG, "%s: thread pool created \n", __FUNCTION__);

	ret = sfs_job_list_init(&jobs);
	if (ret == -1) {
		// Job list creation failed
		// No point in continuing
		// pthread_kill(recv_thread, SIGKILL);
		sstack_transport_deregister(TCPIP, &transport);
		sstack_thread_pool_destroy(fused_thread_pool);

		return NULL;
	}
	sfs_log(fused_ctx, SFS_DEBUG, "%s: job list created \n", __FUNCTION__);
	// Create a dispatcher thread
	if (pthread_attr_init(&dispatcher_attr) == 0) {
		pthread_attr_setscope(&dispatcher_attr, PTHREAD_SCOPE_SYSTEM);
		pthread_attr_setstacksize(&dispatcher_attr, 131072); // 128KiB
		pthread_attr_setdetachstate(&dispatcher_attr, PTHREAD_CREATE_DETACHED);
		ret = pthread_create(&dispatcher_thread, &dispatcher_attr,
						sfs_dispatcher, (void *) jobs);
	}
	if (ret != 0) {
		sfs_log(fused_ctx, SFS_CRIT, "%s: Unable to create job dispatcher "
						"thread\n", __FUNCTION__);
		db->db_ops.db_close(fused_ctx);
		// pthread_kill(recv_thread, SIGKILL);
		sstack_transport_deregister(TCPIP, &transport);
		sstack_thread_pool_destroy(fused_thread_pool);
		(void) sfs_job_queue_destroy(&jobs);
		return NULL;
	}
	sfs_log(fused_ctx, SFS_DEBUG, "%s: Job dispatcher thread created\n",
					__FUNCTION__);
	// Initialize pending job queues
	ret = sfs_job_list_init(&pending_jobs);
	if (ret == -1) {
		// Job list creation failed
		// No point in continuing
		db->db_ops.db_close(fused_ctx);
		// pthread_kill(recv_thread, SIGKILL);
		sstack_transport_deregister(TCPIP, &transport);
		sstack_thread_pool_destroy(fused_thread_pool);
		(void) sfs_job_queue_destroy(&jobs);
		return NULL;
	}
	sfs_log(fused_ctx, SFS_DEBUG, "%s: pending job list created \n",
					__FUNCTION__);

	// Create jobmap tree
	jobmap_tree = jobmap_tree_init();
	if (NULL == jobmap_tree) {
		sfs_log(fused_ctx, SFS_ERR, "%s: Failed to create jobmap RB-tree \n",
						__FUNCTION__);
		db->db_ops.db_close(fused_ctx);
		// pthread_kill(recv_thread, SIGKILL);
		sstack_transport_deregister(TCPIP, &transport);
		sstack_thread_pool_destroy(fused_thread_pool);
		(void) sfs_job_queue_destroy(&jobs);
		(void) sfs_job_queue_destroy(&pending_jobs);
		return NULL;
	}
	sfs_log(fused_ctx, SFS_DEBUG, "%s: job map tree created \n", __FUNCTION__);

	// Create jobid tree
	jobid_tree = jobid_tree_init();
	if (NULL == jobid_tree) {
		sfs_log(fused_ctx, SFS_ERR, "%s: Failed to create jobid RB-tree \n",
						__FUNCTION__);
		db->db_ops.db_close(fused_ctx);
		// pthread_kill(recv_thread, SIGKILL);
		sstack_transport_deregister(TCPIP, &transport);
		sstack_thread_pool_destroy(fused_thread_pool);
		(void) sfs_job_queue_destroy(&jobs);
		(void) sfs_job_queue_destroy(&pending_jobs);
		free(jobmap_tree);
		free(storage_tree);
		return NULL;
	}
	sfs_log(fused_ctx, SFS_DEBUG, "%s: jobid tree created \n", __FUNCTION__);
	// Create job_id bitmap
	fused_job_id_bitmap  = fused_init_bitmap(MAX_OUTSTANDING_JOBS, fused_ctx);
	if (NULL == fused_job_id_bitmap) {
		db->db_ops.db_close(fused_ctx);
		// pthread_kill(recv_thread, SIGKILL);
		sstack_transport_deregister(TCPIP, &transport);
		sstack_thread_pool_destroy(fused_thread_pool);
		(void) sfs_job_queue_destroy(&jobs);
		(void) sfs_job_queue_destroy(&pending_jobs);
		free(jobmap_tree);
		free(jobid_tree);
		free(storage_tree);
	}

	sfs_log(fused_ctx, SFS_DEBUG, "%s: Job bitmap created \n", __FUNCTION__);

	pthread_spin_init(&jobmap_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&jobid_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&filelock_lock, PTHREAD_PROCESS_PRIVATE);

	sfs_log(fused_ctx, SFS_INFO, "%s: FUSED initialization completed\n",
		__FUNCTION__);

	return NULL;
}

static void
fused_print_help(const char *progname)
{

	fprintf(stderr,
		"Usage: %s ipaddr1,branch[,RO/RW,storage_weight][:ipadddr2...] "
		" mountpoint \n"
		" For local directory, use 127.0.0.1 as ipaddr. \n"
		" RO/RW is optional. By default, RO is used \n"
		" Storage weight is optional. By default, medium weight is chosen\n"
		"\n"
		"General options:\n"
		"	-h --help			print help\n"
		"	-V --version		print version\n"
		"	-D=<log_file_dir>	Directory where log files are stored\n"
		"	-L=<log_level>		Logging level\n"
		"\n",
		progname);

}


// Store chunk paths found in a temporary space before populating db
// This is to avoid db initialization before arguments are validated
// Each branch is of type path,RW/RO,weight. Last two parameters are
// optional with RO and DEFAULT_WEIGHT taken if not specified.
static int
fused_store_branch(char *branch)
{
	char *res = NULL;
	char **ptr = NULL;
	char *temp = NULL;

	temp = realloc(fused_chunks,
			(nchunks + 1) * sizeof(sfs_chunk_entry_t));
	ASSERT((temp != NULL), "Memory alocation failed", 0, 1, 0);
	if (NULL == temp)
		exit(-1);
	fused_chunks = (sfs_chunk_entry_t *) temp;
	memset((void *) (fused_chunks + nchunks), '\0', sizeof(sfs_chunk_entry_t));
	ptr = (char **) &branch;
	res = strsep(ptr, ",");
	if (!res)
		return 0;
	strncpy(fused_chunks[nchunks].ipaddr, res, IPV6_ADDR_LEN - 1);
	res = strsep(ptr, ",");
	if (!res)
		return 0;
	fused_chunks[nchunks].chunk_path = strndup(res, PATH_MAX - 1);
	fused_chunks[nchunks].chunk_pathlen = strlen(res) + 1;
	fused_chunks[nchunks].rw = 0;
	fused_chunks[nchunks].weight = DEFAULT_WEIGHT;

	res = strsep(ptr, ",");
	if (res) {
		if (strcasecmp(res, "rw") == 0) {
			fused_chunks[nchunks].rw = 1;
		}
	}

	res = strsep(ptr, ",");
	if (res) {
		uint32_t weight = atoi(res);

		if (weight > MINIMUM_WEIGHT && weight < MAXIMUM_WEIGHT)
			fused_chunks[nchunks].weight = weight;
	}

	return 1;
}

static int
fuxed_remove_branch(char *branch)
{
	char *res = NULL;
	char **ptr = NULL;
	int i = 0;
	char ipaddr[IPV6_ADDR_LEN] = { '\0' };
	char file_path[PATH_MAX] = { '\0' };
	int found = 0;
	char *temp = NULL;

	ptr = (char **) &branch;
	res = strsep(ptr, ",");
	if (!res)
		return 0;

	strncpy(ipaddr, res, IPV6_ADDR_LEN - 1);
	res = strsep(ptr, ",");
	if (!res)
		return 0;
	strncpy(file_path, res, PATH_MAX - 1);

	for (i = 0; i < nchunks; i++) {
		if (!strcmp(ipaddr, fused_chunks[i].ipaddr) &&
			!strcmp(file_path, fused_chunks[i].chunk_path)) {
			found = 1;
			break;
		}
	}
	if (!found)
		return 0;
	// Readjust fused_chunks
	temp = (char *) ((sfs_chunk_entry_t *) (fused_chunks + i)) ;
	// Copy rest of the chunks leaving the current one
	memcpy((void *)temp , (void *) (fused_chunks + i + 1), (nchunks -i -1) *
			sizeof(sfs_chunk_entry_t));
	// Adjust the fused_chunks size
	// If realloc fails, then we have an issue
	temp = realloc(fused_chunks,
			(nchunks - 1) * sizeof(sfs_chunk_entry_t));
	if (NULL == temp) {
		// Realloc failed. Fail the request
		fused_chunks[i].inuse = 0;
		return 0;
	}
	// Update number of chunks
	nchunks -= 1;

	return 1;
}


static int
fused_parse_branches(const char *arg)
{
	char *buf = strdup(arg);
	char **ptr = (char **)&buf;
	char *branch;
	int nbranches = 0;

	while((branch = strsep(ptr, ROOT_SEP)) != NULL) {
		if (strlen(branch) == 0)
			continue;
		nbranches += fused_store_branch(branch);
	}

	free(buf);

	return nbranches;
}

static int
fused_opt_proc(void *data, const char *arg, int key,
		struct fuse_args *outargs)
{
	int res = -1;

	switch(key) {
		case KEY_LOG_FILE_DIR:
			strncpy(sstack_log_directory,
					get_opt_str(arg, "-D="),
				PATH_MAX - 1);
			fprintf(stderr, "%s: Log directory = %s \n", __FUNCTION__,
							sstack_log_directory);
			return 0;
		case KEY_HELP:
			fused_print_help(outargs->argv[0]);
			exit(0);
		case KEY_VERSION:
			fprintf(stderr, "sfs version: %s\n", SSTACK_VERSION);
			exit(0);
		case KEY_LOG_LEVEL:
			sstack_log_level = atoi(get_opt_str(arg, "-L="));
			fprintf(stderr, "%s: Logging level = %d\n",
							__FUNCTION__, sstack_log_level);
			return 0;
		case FUSE_OPT_KEY_NONOPT:
			res = fused_parse_branches(arg);
			if (res > 0)
				return 0;
			else
				return 1;
		default:
			ASSERT((1 == 0), "Invalid argument specified. Exiting",
					0, 0, 0);
			exit(-1);
	}
}

// This structure is pushed to end of the file to avoid function
// declarations
static struct fuse_operations fused_oper = {
	.getattr		=	fused_getattr,
	.readlink		=	fused_readlink,
	.readdir		=	fused_readdir,
	.mknod			=	fused_mknod,
	.mkdir			=	fused_mkdir,
	.unlink			=	fused_unlink,
	.rmdir			=	fused_rmdir,
	.symlink		=	fused_symlink,
	.rename			=	fused_rename,
	.link			=	fused_link,
	.chmod			=	fused_chmod,
	.chown			=	fused_chown,
	.truncate		=	fused_truncate,
	.open			=	fused_open,
	.read			=	fused_read,
	.write			=	fused_write,
	.statfs			=	fused_statfs,
	.release		=	fused_release,
	.fsync			=	fused_fsync,
	.setxattr		=	fused_setxattr,
	.getxattr		=	fused_getxattr,
	.listxattr		=	fused_listxattr,
	.removexattr	=	fused_removexattr,
	.init			=	fused_init,
	.destroy		=	fused_destroy,
	.access			=	fused_access,
	.create			=	fused_create,
	.ftruncate		=	fused_ftruncate,
	.utimens		=	fused_utimens,
	.bmap			=	fused_bmap,	// Required?? Similar to FIBMAP
	.ioctl			=	fused_ioctl,
	.poll			=	fused_poll,
	.write_buf		=	NULL,//fused_write_buf, 	// Similar to write
	.read_buf		=	NULL, //fused_read_buf,	// Similar to read
};

int
main(int argc, char *argv[])
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	int uid = getuid();
	int gid = getgid();
	int ret = -1;
	int i = 0;

	// Check if root is mounting the file system.
	// If so, return error
	if (uid == 0 && gid == 0) {
		fprintf(stderr, "FUSED: Mounting FUSED by root opens security holes."
			" Please try mounting as a normal user\n");
		return -1;
	}
	// Option parsing
	ret = fuse_opt_parse(&args, NULL, fused_opts, fused_opt_proc);
	ASSERT((ret != -1), "Parsing arguments failed. Exiting ...", 0, 1, -1);

	// Initialize logging
	fused_ctx = sfs_create_log_ctx();
	if (NULL == fused_ctx) {
		fprintf(stderr, "%s: Log ctx creation failed. Logging disable",
			__FUNCTION__);
	} else {
		ret = sfs_log_init(fused_ctx, sstack_log_level, "sfs");
		ASSERT((ret == 0), "Log initialization failed. Logging disabled",
			0, 0, 0);
	}

	strcpy(fused_mountpoint, args.argv[args.argc - 1]);
	fprintf(stderr, "%s: File system mount point = %s \n",
					__FUNCTION__, fused_mountpoint);

	ret = fuse_opt_add_arg(&args, "-obig_writes");
	ASSERT((ret != -1), "Enabling big writes failed.", 0, 0, 0);
#if 0
	for (i = 0; i < args.argc; i++) {
		fprintf(stderr, "%d %s \n", i, args.argv[i]);
	}
#endif

	return fuse_main(args.argc, args.argv, &fused_oper, NULL);
}

