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
#include <sstack_db.h>
#include <sstack_log.h>
#include <sstack_helper.h>
#include <sstack_version.h>
#include <sstack_bitops.h>
#include <sstack_cache_api.h>
#include <sstack_transport_tcp.h>
#include <sstack_serdes.h>
#include <policy.h>
#include <sfs_internal.h>
#include <sfs.h>
#include <sfs_entry.h>
#include <sfs_job.h>
#include <mongo_db.h>
#include <sfs_jobid_tree.h>
#include <sfs_jobmap_tree.h>
#include <sfs_lock_tree.h>

/* Macros */
#define MAX_INODE_LEN 40 // Maximum len of uint64_t is 39
#define MAX_JOB_SUBMIT_RETRIES 10 // Maximum number of retries
#define MAX_HIGH_PRIO 64
#define MAX_MEDIUM_PRIO 16
#define MAX_LOW_PRIO 4
#define SFS_JOB_SLEEP 5

/* BSS */
log_ctx_t *sfs_ctx = NULL;
char sstack_log_directory[PATH_MAX] = { '\0' };
sfs_log_level_t sstack_log_level = SFS_DEBUG;
sfs_chunk_entry_t	*sfs_chunks = NULL;
uint64_t nchunks = 0;
db_t *db = NULL;
memcached_st *mc = NULL;
pthread_mutex_t inode_mutex = PTHREAD_MUTEX_INITIALIZER;
unsigned long long inode_number = INODE_NUM_START;
unsigned long long active_inodes = 0;
sstack_client_handle_t sfs_handle = 0;
sstack_thread_pool_t *sfs_thread_pool = NULL;
sstack_transport_t transport;
jobmap_tree_t *jobmap_tree = NULL;
jobid_tree_t *jobid_tree = NULL;
filelock_tree_t *filelock_tree = NULL;
pthread_spinlock_t jobmap_lock;
pthread_spinlock_t jobid_lock;
pthread_spinlock_t filelock_lock;
extern sstack_bitmap_t *sstack_job_id_bitmap;

/*
 * jobs is the job list of unsubmitted (to sfsd) jobs
 * pending_jobs is the list of jobs waiting for completion
 */
sfs_job_queue_t *jobs = NULL;
sfs_job_queue_t *pending_jobs = NULL;
sstack_sfsd_pool_t *sfsd_pool = NULL;
// Slabs
bds_cache_desc_t sfs_global_cache[MAX_CACHE_OFFSET];


/* Structure definitions */

static struct fuse_opt sfs_opts[] = {
	FUSE_OPT_KEY("log_file_dir=%s", KEY_LOG_FILE_DIR),
	FUSE_OPT_KEY("branches=%s", KEY_BRANCHES),
	FUSE_OPT_KEY("--help", KEY_HELP),
	FUSE_OPT_KEY("-h", KEY_HELP),
	FUSE_OPT_KEY("--version", KEY_VERSION),
	FUSE_OPT_KEY("-v", KEY_VERSION),
	FUSE_OPT_KEY("log_level=%d", KEY_LOG_LEVEL),
	FUSE_OPT_END
};

struct sfs_cache_entry slabs[MAX_CACHE_OFFSET] = {
	{"payload-cache", sizeof(sstack_payload_t)},
};

/* Forward declarations */
static int sfs_store_branch(char *branch);
static int sfs_remove_branch(char *branch);


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

    sfs_log(sfs_ctx, SFS_DEBUG, "%s: path = %s\n", __FUNCTION__, path);
    if (lstat(path, &status) == -1) {
		sfs_log(sfs_ctx, SFS_ERR,
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
	sfs_log(sfs_ctx, SFS_INFO,
		"%s: nlinks for %s are %d\n", __FUNCTION__, path, inode.i_links);
#if 0
	// Populate size of each extent
	policy = get_policy(path);
	if (NULL == policy) {
		sfs_log(sfs_ctx, SFS_INFO,
			"%s: No policies specified. Default policy applied\n",
			__FUNCTION__);
		inode.i_policy.pe_attr.a_qoslevel = QOS_LOW;
		inode.i_policy.pe_attr.a_ishidden = 0;
	} else {
		// Got the policy
		sfs_log(sfs_ctx, SFS_INFO,
			"%s: Got policy for file %s\n", __FUNCTION__, path);
		sfs_log(sfs_ctx, SFS_INFO, "%s: ver %s qoslevel %d hidden %d \n",
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
		extent.e_cksum = sstack_checksum(sfs_ctx, path); // CRC32
		sfs_log(sfs_ctx, SFS_INFO, "%s: checksum of %s is %lu \n",
			__FUNCTION__, path, extent.e_cksum);
	} else
		extent.e_cksum = 0; // Place holder

	ep = malloc(sizeof(sstack_file_handle_t));
	if (NULL == ep) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Allocation failed for e_path\n",
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
	ret = put_inode(&inode, db);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to store inode #%lld for "
				"path %s. Error %d\n", __FUNCTION__, inode.i_num,
				inode.i_name, errno);
		return -errno;
	}

	free(ep);
	// Store inode <-> path into reverse lookup
	sprintf(inode_str, "%lld", inode.i_num);
	ret = sstack_memcache_store(mc, path, inode_str, (strlen(inode_str) + 1),
					sfs_ctx);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to store object into memcached."
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
	sfs_log(sfs_ctx, SFS_ERR, "%s: dir_name = %s \n", __FUNCTION__,
			dir_name);

	d = opendir (dir_name);

	// Check it was opened
	if (! d) {
		sfs_log (sfs_ctx,SFS_ERR,
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
			sfs_log(sfs_ctx, SFS_ERR, "%s: dir_name = %s, d_name = %s \n",
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
					sfs_log (sfs_ctx, SFS_ERR,
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
		sfs_log (sfs_ctx, SFS_ERR, "%s: Could not close '%s': %s\n",
			__FUNCTION__, dir_name, strerror (errno));
		exit (-1);
	}
}

#if 0
void
del_branch(char *path)
{
}


static void
handle(sfs_client_request_t *req)
{
	char *buf;
	char **ptr = (char **) &buf;
	char *branch = NULL;

	syslog(LOG_INFO, "req_type = %d \n", req->hdr.type);
	if (req->hdr.type == ADD_BRANCH || req->hdr.type == DEL_BRANCH) {
		buf = strdup(req->u1.branches);
		syslog(LOG_INFO, "new branch = %s \n", req->u1.branches);
		while((branch = strsep(ptr, ROOT_SEP)) != NULL) {
			if (strlen(branch) == 0) continue;
			if (req->hdr.type == ADD_BRANCH)
				sfs_store_branch(branch);
			else if (req->hdr.type == DEL_BRANCH)
				sfs_remove_branch(branch);
		}
		free(buf);

		populate_db(sfs_chunks[nchunks].chunk_path);
	} else if (req->hdr.type == ADD_POLICY ||
			req->hdr.type == DEL_POLICY) {
			//			 TBD
			// Call add_policy which should add/replace/delete the policy
			// mentioned in the request to the appropriate policy bucket
			// and add/replace/delete the binary entry in POLICY_FILE
	}
}

static void
deserialize(sfs_client_request_t *req, char *buf)
{
	unsigned int value;
	size_t size = 0;

	memcpy(&value, buf, sizeof(int));
	req->hdr.magic = ntohl(value);
	size += sizeof(int);
	memcpy(&value, buf + size, sizeof(int));
	req->hdr.type = ntohl(value);
	size += sizeof(int);
	if (req->hdr.type == ADD_BRANCH || req->hdr.type == DEL_BRANCH) {
		memcpy(req->u1.branches, buf + size, BRANCH_MAX);
		size += BRANCH_MAX;
		memcpy(req->u1.login_name, buf + size, LOGIN_NAME_MAX);
		size += LOGIN_NAME_MAX;
	} else if (req->hdr.type == ADD_POLICY ||
			req->hdr.type == DEL_POLICY) {

		memcpy(req->u2.fname, buf + size, PATH_MAX);
		size += PATH_MAX;
		memcpy(req->u2.ftype, buf + size, TYPENAME_MAX);
		size += TYPENAME_MAX;
		memcpy(&value, buf + size, sizeof(int));
		req->u2.uid = ntohl(value);
		size += sizeof(int);
		memcpy(&value, buf + size, sizeof(int));
		req->u2.gid = ntohl(value);
		size += sizeof(int);
		memcpy(&value, buf + size, sizeof(int));
		req->u2.hidden = ntohl(value);
		size += sizeof(int);
		memcpy(&value, buf + size, sizeof(int));
		req->u2.qoslevel = ntohl(value);
		size += sizeof(int);
	}
}

static void
sigchld_handler(int signal)
{
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

#endif

static void
sfs_process_read_response(sstack_payload_t *payload)
{
	sstack_nfs_response_struct resp = payload->response_struct;
	// struct sstack_nfs_read_resp read_resp = resp.read_resp;
	sstack_jm_t			*jm_node = NULL, jm_key;
	sstack_jt_t			*jt_node = NULL, jt_key;
	pthread_t			thread_id;
	sstack_job_map_t	*job_map = NULL;
	sfs_job_t			*job = NULL;
	sstack_job_id_t		job_id;
	sstack_inode_t 		inode;
	sfsd_t				*sfsd = NULL;
	int					i = 0, ret = -1;

	job_id = payload->hdr.job_id;

	jt_key.magic = JTNODE_MAGIC;
	jt_key.job_id = job_id;
	jt_node = jobid_tree_search(jobid_tree, &jt_key);

	if (jt_node == NULL) {
		/* TBD: something wrong happened.
		 * Should we assert or just return.
		 * For now just return
		 */
		errno = SSTACK_CRIT_FAILURE;
		return;
	}
	thread_id = jt_node->thread_id;
	job = jt_node->job;

	jm_key.magic = JMNODE_MAGIC;
	jm_key.thread_id = thread_id;
	jm_node = jobmap_tree_search(jobmap_tree, &jm_key);

	if (jm_node == NULL) {
		/* this is not an error. This could happen if earlier
		   job in this job_map had a read error due to which
		   removed the job_map from the tree and returned an
		   error to the application thread;
		 */
		return;
	}
	job_map = jm_node->job_map;

	if (resp.command_ok == SSTACK_SUCCESS) {
		pthread_spin_lock(&job_map->lock);
        job_map->num_jobs_left --;
        pthread_spin_unlock(&job_map->lock);

		job->payload = payload;

		if (job_map->num_jobs_left == 0) {
			pthread_cond_signal(&job_map->condition);
		}
	} else if ((resp.command_ok == -SSTACK_ECKSUM) ||
						(resp.command_ok == -SSTACK_NOMEM)) {
		uint64_t	inode_num;

		inode_num = payload->command_struct.read_cmd.inode_no;
		ret = get_inode(inode_num, &inode, db);
		if (ret != 0) {
			sfs_log(sfs_ctx, SFS_ERR, "%s: Error get inode %lld, error = %s ",
									__FUNCTION__, inode_num, ret);
			/* TBD: ASSERT or return. For now return */
			errno = SSTACK_CRIT_FAILURE;
			return;
		}

		/* If read_ecode set and we get an error response, then both
		 * replicas and erasure code didn't save us. If DR is enabled
		 * for this file, then fetch it from DR
		 */
		if (payload->command_struct.read_cmd.read_ecode) {
			if (inode.i_enable_dr) {
				/* TBD: Do we fetch the entire file or just this extent*/
			}
		} else {
			/* Fetching from replica has precedence over erasure code */
			for (i = 0; i < inode.i_numreplicas; i++) {
				if (inode.i_sfsds[i].sfsd->handle ==
									job->sfsds[0]->handle) {
					sfsd = inode.i_sfsds[(i+1) % inode.i_numreplicas].sfsd;
					if (sfsd->handle == inode.i_primary_sfsd->sfsd->handle) {
						payload->command_struct.read_cmd.read_ecode = 1;
						job->payload = payload;
						job->sfsds[0] = sfsd;
					} else {
						job->sfsds[0] = sfsd;
					}
				}
			}

			/* enqueue the job to the job_list */
			ret = sfs_submit_job(job->priority, jobs, job);
			if (ret == -1) {
				errno = SSTACK_CRIT_FAILURE;
			}
		}
	} else {
		/* Read errors */
		pthread_spin_lock(&job_map->lock);
        job_map->err_no = resp.command_ok;
        pthread_spin_unlock(&job_map->lock);
		pthread_cond_signal(&job_map->condition);
	}
}

/*
 * sfs_process_payload - Process received payload
 *
 * arg - payload structure
 *
 * This is called in worker thread context. Once the payload is handled,
 * thread waiting on the job completion is woken up.
 */

static void*
sfs_process_payload(void *arg)
{
	sstack_payload_t *payload = (sstack_payload_t *) arg;

	switch (payload->command) {
		case (NFS_READ_RSP):
			sfs_process_read_response(payload);
			break;

		case (NFS_ESURE_CODE_RSP):
			//sfs_process_esure_code_response(payload);
			break;

		default:
			break;
	}

	return NULL;
}

/*
 * sfs_dispatcher - Job dispatcher thread
 *
 * arg - job queue list which contains unsubmitted jobs
 * Reads the job queue and processes the requests
 * Sends the IOs in following order in every loop iteration:
 * Process MAX_HIGH_PRIO jobs first
 * Process MAX_MEDIUM_PRIO jobs next
 * Process MAX_LOW_PRIO jobs last
 * These constants are chosen to honour QoS setting per file
 */
static void *
sfs_dispatcher(void * arg)
{
	sfs_job_queue_t *job_list = (sfs_job_queue_t *) arg;
	bds_list_head_t temp = NULL;

	// Parameter validation
	if (NULL == arg) {
		// This is to catch BSS corruption only.
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);
		errno = EINVAL;
		return NULL;
	}

	while(1) {
		int i = 0;
		sfs_job_t *job = NULL;
		int j = 0;
		sfsd_t *sfsd = NULL;
		int ret = -1;

		// TODO
		// The following can be converted to a macro

		// Process hi priority queue
		temp = &job_list->list;
		list_for_each_entry(job, temp, wait_list) {
			if (i == MAX_HIGH_PRIO)
				break;
			// Detach the job
			bds_list_del((bds_list_head_t) &job->wait_list);
			// process the job
			for (j = 0; j < job->num_clients; j++) {
				sfsd = (sfsd_t *) &job->sfsds[j];
				ret = sstack_send_payload(sfsd->handle, job->payload,
							sfsd->transport, job->id, job, HIGH_PRIORITY,
							sfs_ctx);
				if (ret != 0) {
					// TBD
					// Handle failure
				}
				job->job_status[j] = JOB_STARTED;
			}
			i++;
			ret = sfs_enqueue_job(HIGH_PRIORITY, pending_jobs, job);
			if (ret != 0) {
				// TODO
				// Handle failure
				// This can happen only if there is memory corruption
			}
		}

		// Process medium priority queue
		i = 0;
		temp ++;
		list_for_each_entry(job, temp, wait_list) {
			if (i == MAX_MEDIUM_PRIO)
				break;
			// Detach the job
			bds_list_del((bds_list_head_t) &job->wait_list);
			// process the job
			for (j = 0; j < job->num_clients; j++) {
				sfsd = (sfsd_t *) &job->sfsds[j];
				ret = sstack_send_payload(sfsd->handle, job->payload,
							sfsd->transport, job->id, job, MEDIUM_PRIORITY,
							sfs_ctx);
				if (ret != 0) {
					// TBD
					// Handle failure
				}
				job->job_status[j] = JOB_STARTED;
			}
			i++;
			ret = sfs_enqueue_job(MEDIUM_PRIORITY, pending_jobs, job);
			if (ret != 0) {
				// TODO
				// Handle failure
				// This can happen only if there is memory corruption
			}
		}
		// Process low priority queue
		i = 0;
		temp ++;
		list_for_each_entry(job, temp, wait_list) {
			if (i == MAX_LOW_PRIO)
				break;
			// Detach the job
			bds_list_del((bds_list_head_t) &job->wait_list);
			// process the job
			for (j = 0; j < job->num_clients; j++) {
				sfsd = (sfsd_t *) &job->sfsds[j];
				ret = sstack_send_payload(sfsd->handle, job->payload,
							sfsd->transport, job->id, job, LOW_PRIORITY,
							sfs_ctx);
				if (ret != 0) {
					// TBD
					// Handle failure
				}
				job->job_status[j] = JOB_STARTED;
			}
			i++;
			ret = sfs_enqueue_job(LOW_PRIORITY, pending_jobs, job);
			if (ret != 0) {
				// TODO
				// Handle failure
				// This can happen only if there is memory corruption
			}
		}

		sleep(SFS_JOB_SLEEP); // Yield
	}

	return NULL;
}

/*
 * sfs_handle_connction - Thread function to listen to requests from sfsds
 *
 * arg - argument (not used)
 *
 * Never returns unless an error. On error, returns NULL.
 */

static void *
sfs_handle_connection(void * arg)
{
	int ret = -1;
	uint32_t mask = READ_BLOCK_MASK;
	sstack_payload_t *payload;
	int num_retries = 0;

	while (1) {
		ret = transport.transport_ops.select(sfs_handle, mask);
		if (ret != READ_NO_BLOCK) {
			sfs_log(sfs_ctx, SFS_INFO, "%s: Connection down. Waiting for "
							"retry \n", __FUNCTION__);
			sleep(1);
		}

		// Receive a payload
		payload = sstack_recv_payload(sfs_handle, &transport, sfs_ctx);
		if (NULL == payload) {
			sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to receive payload. "
							"Error = %d\n", __FUNCTION__, errno);
			continue;
		}

retry:
		ret = sstack_thread_pool_queue(sfs_thread_pool, sfs_process_payload,
						(void *)payload);
		if (ret != 0) {
			sfs_log(sfs_ctx, SFS_ERR, "%s: Queueing job failed. "
							"Error = %d \n", __FUNCTION__, errno);
			// Only ENOMEM is the possible errno
			// Wait for sometime and retry back
			sleep(2);
			num_retries ++;
			if (num_retries < MAX_JOB_SUBMIT_RETRIES)
				goto retry;
			else {
				sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to submit job to thread "
								"pool after %d retries. Quitting ...\n",
								__FUNCTION__, num_retries);
				return NULL;
			}
		}
	}

	// Ideally control should never reach here

	return NULL;
}

/*
 * sfs_init_thread_pool - Create a thread pool to handle jobs
 *
 * Create thread pool with minimum 4 threads and maximum of number of
 * entries in listen queue (currently 1024).
 *
 * Returns 0 on success and -1 on failure.
 */

static int
sfs_init_thread_pool(void)
{
	pthread_attr_t attr;

	// Create thread pool to handle sfsd requests/responses
	pthread_attr_init(&attr);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	pthread_attr_setstacksize(&attr, 65536);
	// Create SSTACK_BACKLOG threads at max
	// Current value is 1024 which is quite big
	sfs_thread_pool = sstack_thread_pool_create(4, SSTACK_BACKLOG, 30, &attr);
	if (NULL == sfs_thread_pool) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to create sfs threadpool. "
					"Connection handling is implausible. Error = %d \n ",
					__FUNCTION__, errno);
		return -1;
	}

	sfs_log(sfs_ctx, SFS_INFO, "%s: SFS thread pool successfully created \n",
						__FUNCTION__);

	return 0;
}



static void *
sfs_init(struct fuse_conn_info *conn)
{
	pthread_t thread;
	pthread_attr_t attr;
	pthread_t recv_thread;
	pthread_attr_t recv_attr;
	pthread_t dispatcher_thread;
	pthread_attr_t dispatcher_attr;
	int ret = -1;
	int chunk_index = 0;
	sstack_transport_ops_t ops;
	sstack_transport_type_t type;
	char *intf_addr = NULL;
	int i = 0;

	// Create a thread to handle sfs<->sfsd communication
	if(pthread_attr_init(&recv_attr) == 0) {
		pthread_attr_setscope(&recv_attr, PTHREAD_SCOPE_SYSTEM);
		pthread_attr_setstacksize(&recv_attr, 131072); // 128KiB
		pthread_attr_setdetachstate(&recv_attr, PTHREAD_CREATE_DETACHED);
		ret = pthread_create(&recv_thread, &recv_attr,
						sfs_handle_connection, NULL);
	}

	ASSERT((ret == 0),"Unable to create thread to handle sfs<->sfsd comm",
			0, 0, 0);
	// Create a dispatcher thread
	if (pthread_attr_init(&dispatcher_attr) == 0) {
		pthread_attr_setscope(&dispatcher_attr, PTHREAD_SCOPE_SYSTEM);
		pthread_attr_setstacksize(&dispatcher_attr, 131072); // 128KiB
		pthread_attr_setdetachstate(&dispatcher_attr, PTHREAD_CREATE_DETACHED);
		ret = pthread_create(&dispatcher_thread, &dispatcher_attr,
						sfs_dispatcher, (void *) jobs);
	}
	ASSERT((ret == 0),"Unable to create job dispatcher thread", 0, 0, 0);

	// Create db instance
	db = create_db();
	ASSERT((db != NULL), "Unable to create db. FATAL ERROR", 0, 1, NULL);
	db_register(db, mongo_db_init, mongo_db_open, mongo_db_close,
		mongo_db_insert, mongo_db_remove, mongo_db_iterate, mongo_db_get,
		mongo_db_seekread, mongo_db_update, mongo_db_delete,
		mongo_db_cleanup, sfs_ctx);
	if (db->db_ops.db_init(sfs_ctx) != 0) {
		sfs_log(sfs_ctx, SFS_CRIT, "%s: DB init faled with error %d\n",
			__FUNCTION__, errno);
		return NULL;
	}

	// Other init()s go here
	// Initialize TCP transport
	type = TCPIP;
	ops.rx = tcp_rx;
	ops.tx = tcp_tx;
	ops.client_init = tcp_client_init;
	ops.server_setup = tcp_server_setup;
	transport.transport_hdr.tcp.ipv4 = 1; // IPv4 adress for now
	transport.transport_ops = ops;
	// get local ip address
	// eth0 is the assumed interface

	ret = get_local_ip("eth0", intf_addr, IPv4);
	if (ret == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Please enable eth0 interface "
						"and retry.\n", __FUNCTION__);
		return NULL;
	}

	strcpy((char *) &transport.transport_hdr.tcp.ipv4_addr, intf_addr);
	transport.transport_hdr.tcp.port = SFS_SERVER_PORT;
	ret = sstack_transport_register(type, &transport, ops);
	// Call server setup
	sfs_handle = transport.transport_ops.server_setup(&transport);
	if (sfs_handle == -1) {
		// Server socket creation failed.
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to create sfs socket. "
					"Error = %d \n", __FUNCTION__, errno);
		// cleanup
		db->db_ops.db_close(sfs_ctx);
		pthread_kill(recv_thread, SIGKILL);
		sstack_transport_deregister(type, &transport);

		return NULL;
	}

	ret = sfs_init_thread_pool();
	if (ret != 0) {
		// Thread pool creation failed.
		// Exit out as we can't handle requests anyway
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to create thread pool. "
						"Exiting ...\n", __FUNCTION__);
		db->db_ops.db_close(sfs_ctx);
		pthread_kill(recv_thread, SIGKILL);
		sstack_transport_deregister(type, &transport);

		return NULL;
	}

	// Initialize job queues
	ret = sfs_job_list_init(&jobs);
	if (ret == -1) {
		// Job list creation failed
		// No point in continuing
		db->db_ops.db_close(sfs_ctx);
		pthread_kill(recv_thread, SIGKILL);
		sstack_transport_deregister(type, &transport);
		sstack_thread_pool_destroy(sfs_thread_pool);

		return NULL;
	}
	// Initialize pending job queues
	ret = sfs_job_list_init(&pending_jobs);
	if (ret == -1) {
		// Job list creation failed
		// No point in continuing
		db->db_ops.db_close(sfs_ctx);
		pthread_kill(recv_thread, SIGKILL);
		sstack_transport_deregister(type, &transport);
		sstack_thread_pool_destroy(sfs_thread_pool);
		(void) sfs_job_queue_destroy(jobs);
		return NULL;
	}

	// Create jobmap tree
	jobmap_tree = jobmap_tree_init();
	if (NULL == jobmap_tree) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to create jobmap RB-tree \n",
						__FUNCTION__);
		db->db_ops.db_close(sfs_ctx);
		pthread_kill(recv_thread, SIGKILL);
		sstack_transport_deregister(type, &transport);
		sstack_thread_pool_destroy(sfs_thread_pool);
		(void) sfs_job_queue_destroy(jobs);
		return NULL;
	}

	// Create jobid tree
	jobid_tree = jobid_tree_init();
	if (NULL == jobid_tree) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to create jobid RB-tree \n",
						__FUNCTION__);
		db->db_ops.db_close(sfs_ctx);
		pthread_kill(recv_thread, SIGKILL);
		sstack_transport_deregister(type, &transport);
		sstack_thread_pool_destroy(sfs_thread_pool);
		(void) sfs_job_queue_destroy(jobs);
		free(jobmap_tree);
		return NULL;
	}
	// Create job_id bitmap
	ret  = sfs_init_bitmap(sstack_job_id_bitmap,
					MAX_OUTSTANDING_JOBS, sfs_ctx);
	if (ret == -1) {
		db->db_ops.db_close(sfs_ctx);
		pthread_kill(recv_thread, SIGKILL);
		sstack_transport_deregister(type, &transport);
		sstack_thread_pool_destroy(sfs_thread_pool);
		(void) sfs_job_queue_destroy(jobs);
		free(jobmap_tree);
		free(jobid_tree);
	}


	// Create filelock tree
	filelock_tree = filelock_tree_init();
	if (NULL == filelock_tree) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to create filelock "
						" RB-tree \n", __FUNCTION__);
		db->db_ops.db_close(sfs_ctx);
		pthread_kill(recv_thread, SIGKILL);
		sstack_transport_deregister(type, &transport);
		sstack_thread_pool_destroy(sfs_thread_pool);
		(void) sfs_job_queue_destroy(jobs);
		free(jobmap_tree);
		free(jobid_tree);
		free(sstack_job_id_bitmap);
		return NULL;
	}

	pthread_spin_init(&jobmap_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&jobid_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_spin_init(&filelock_lock, PTHREAD_PROCESS_PRIVATE);


	// Initialize sfsd_pool
	sfsd_pool = sstack_sfsd_pool_init();
	if (NULL == sfsd_pool) {
		// sfsd_pool creation failed.
		// Product is not scalable and useless.
		// Exit
		db->db_ops.db_close(sfs_ctx);
		pthread_kill(recv_thread, SIGKILL);
		sstack_transport_deregister(type, &transport);
		sstack_thread_pool_destroy(sfs_thread_pool);
		(void) sfs_job_queue_destroy(jobs);
		(void) sfs_job_queue_destroy(pending_jobs);
		free(jobmap_tree);
		free(jobid_tree);
		free(filelock_tree);
		free(sstack_job_id_bitmap);
		pthread_spin_destroy(&jobmap_lock);
		pthread_spin_destroy(&jobid_lock);
		pthread_spin_destroy(&filelock_lock);
		return NULL;
	}

	// Create pool for payload structure
	for (i = 0; i < MAX_CACHE_OFFSET; i++) {
		int ret = -1;

		ret = bds_cache_create(slabs[i].name, slabs[i].size, 0, NULL,
						NULL, &sfs_global_cache[i]);
		if (ret != 0) {
			sfs_log(sfs_ctx, SFS_ERR, "%s: Could not allocate cache for %s\n",
							slabs[i].name);
			db->db_ops.db_close(sfs_ctx);
			pthread_kill(recv_thread, SIGKILL);
			sstack_transport_deregister(type, &transport);
			sstack_thread_pool_destroy(sfs_thread_pool);
			(void) sfs_job_queue_destroy(jobs);
			(void) sfs_job_queue_destroy(pending_jobs);
			free(jobmap_tree);
			free(jobid_tree);
			free(filelock_tree);
			free(sstack_job_id_bitmap);
			pthread_spin_destroy(&jobmap_lock);
			pthread_spin_destroy(&jobid_lock);
			pthread_spin_destroy(&filelock_lock);
			return NULL;
		}
	}



	// Populate the INODE collection of the DB with all the files found
	// in chunks added so far
	for (chunk_index = 0; chunk_index < nchunks; chunk_index++)
		populate_db(sfs_chunks[chunk_index].chunk_path);

	sfs_log(sfs_ctx, SFS_INFO, "%s: SFS initialization completed\n",
		__FUNCTION__);

	return NULL;
}

static void
sfs_print_help(const char *progname)
{

	fprintf(stderr,
	"Usage: %s [options] branch[,RO/RW,weight][:branch...] mountpoint\n"
	"The first argement is a colon separated list of client directories\n"
	"\n"
	"general options:\n"
	"	-o opt,[opt...]		Mount options\n"
	"	-h --help			print help\n"
	"	-V --version		print version\n"
	"\n"
	"Mount options:\n"
	"	-o log_file_dir		Directory where log files are stored\n"
	"	-o dirs=branch[,RO/RW,weight][:branch...]\n"
	"	alternate way to specify client directories\n"
	"\n",
	progname);
}

// Store chunk paths found in a temporary space before populating db
// This is to avoid db initialization before arguments are validated
// Each branch is of type path,RW/RO,weight. Last two parameters are
// optional with RO and DEFAULT_WEIGHT taken if not specified.
static int
sfs_store_branch(char *branch)
{
	char *res = NULL;
	char **ptr = NULL;
	char *temp = NULL;

	temp = realloc(sfs_chunks,
			(nchunks + 1) * sizeof(sfs_chunk_entry_t));
	ASSERT((temp != NULL), "Memory alocation failed", 0, 1, 0);
	if (NULL == temp)
		exit(-1);
	sfs_chunks = (sfs_chunk_entry_t *) temp;
	memset((void *) (sfs_chunks + nchunks), '\0', sizeof(sfs_chunk_entry_t));
	ptr = (char **) &branch;
	res = strsep(ptr, ",");
	if (!res)
		return 0;
	strncpy(sfs_chunks[nchunks].ipaddr, res, IPV6_ADDR_LEN - 1);
	sfs_log(sfs_ctx, SFS_DEBUG, "%s: ipaddr = %s \n", __FUNCTION__,
					sfs_chunks[nchunks].ipaddr);
	res = strsep(ptr, ",");
	if (!res)
		return 0;
	sfs_chunks[nchunks].chunk_path = strndup(res, PATH_MAX - 1);
	sfs_log(sfs_ctx, SFS_DEBUG, "%s: chunk_path = %s \n", __FUNCTION__,
					sfs_chunks[nchunks].chunk_path);
	sfs_chunks[nchunks].chunk_pathlen = strlen(res) + 1;
	sfs_chunks[nchunks].rw = 0;
	sfs_chunks[nchunks].weight = DEFAULT_WEIGHT;

	res = strsep(ptr, ",");
	if (res) {
		if (strcasecmp(res, "rw") == 0) {
			sfs_chunks[nchunks].rw = 1;
			sfs_log(sfs_ctx, SFS_DEBUG, "%s: rw = %d \n", __FUNCTION__,
							sfs_chunks[nchunks].rw);
		}
	}

	res = strsep(ptr, ",");
	if (res) {
		uint32_t weight = atoi(res);

		if (weight > MINIMUM_WEIGHT && weight < MAXIMUM_WEIGHT)
			sfs_chunks[nchunks].weight = weight;
		sfs_log(sfs_ctx, SFS_DEBUG, "%s: weight = %d \n", __FUNCTION__,
						sfs_chunks[nchunks].weight);
	}

	return 1;
}

static int
sfs_remove_branch(char *branch)
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
		if (!strcmp(ipaddr, sfs_chunks[i].ipaddr) &&
			!strcmp(file_path, sfs_chunks[i].chunk_path)) {
			found = 1;
			break;
		}
	}
	if (!found)
		return 0;
	// Readjust sfs_chunks
	temp = (char *) ((sfs_chunk_entry_t *) (sfs_chunks + i)) ;
	// Copy rest of the chunks leaving the current one
	memcpy((void *)temp , (void *) (sfs_chunks + i + 1), (nchunks -i -1) *
			sizeof(sfs_chunk_entry_t));
	// Adjust the sfs_chunks size
	// If realloc fails, then we have an issue
	temp = realloc(sfs_chunks,
			(nchunks - 1) * sizeof(sfs_chunk_entry_t));
	if (NULL == temp) {
		// Realloc failed. Fail the request
		sfs_chunks[i].inuse = 0;
		return 0;
	}
	// Update number of chunks
	nchunks -= 1;

	return 1;
}


static int
sfs_parse_branches(const char *arg)
{
	char *buf = strdup(arg);
	char **ptr = (char **)&buf;
	char *branch;
	int nbranches = 0;

	while((branch = strsep(ptr, ROOT_SEP)) != NULL) {
		if (strlen(branch) == 0)
			continue;
		sfs_log(sfs_ctx, SFS_DEBUG, "%s: branch = %s\n", __FUNCTION__, branch);
		nbranches += sfs_store_branch(branch);
		sfs_log(sfs_ctx, SFS_DEBUG, "%s: nbranches = %d\n",
						__FUNCTION__, nbranches);
	}

	free(buf);

	return nbranches;
}

static int
sfs_opt_proc(void *data, const char *arg, int key,
		struct fuse_args *outargs)
{
	int res = -1;

	switch(key) {
		case FUSE_OPT_KEY_NONOPT:
			res = sfs_parse_branches(arg);
			if (res > 0)
				return 0;
			else
				return 1;
		case KEY_BRANCHES:
			res = sfs_parse_branches(arg + 9);
			if (res > 0)
				return 0;
			else
				return 1;
		case KEY_LOG_FILE_DIR:
			strncpy(sstack_log_directory,
					get_opt_str(arg, "log_file_dir"),
				PATH_MAX - 1);
			return 0;
		case KEY_HELP:
			sfs_print_help(outargs->argv[0]);
			exit(0);
		case KEY_VERSION:
			fprintf(stderr, "sfs version: %s\n", SSTACK_VERSION);
			exit(0);
		case KEY_LOG_LEVEL:
			sstack_log_level = atoi(get_opt_str(arg, "log_level"));
			return 0;
		default:
			ASSERT((1 == 0), "Invalid argument specified. Exiting",
					0, 0, 0);
			exit(-1);
	}
}

// This structure is pushed to end of the file to avoid function
// declarations
static struct fuse_operations sfs_oper = {
	.getattr		=	sfs_getattr,
	.readlink		=	sfs_readlink,
	.readdir		=	sfs_readdir,
	.mknod			=	sfs_mknod,
	.mkdir			=	sfs_mkdir,
	.unlink			=	sfs_unlink,
	.rmdir			=	sfs_rmdir,
	.symlink		=	sfs_symlink,
	.rename			=	sfs_rename,
	.link			=	sfs_link,
	.chmod			=	sfs_chmod,
	.chown			=	sfs_chown,
	.truncate		=	sfs_truncate,
	.open			=	sfs_open,
	.read			=	sfs_read,
	.write			=	sfs_write,
	.statfs			=	sfs_statfs,
	.release		=	sfs_release,
	.fsync			=	sfs_fsync,
	.setxattr		=	sfs_setxattr,
	.getxattr		=	sfs_getxattr,
	.listxattr		=	sfs_listxattr,
	.removexattr	=	sfs_removexattr,
	.init			=	sfs_init,
	.destroy		=	sfs_destroy,
	.access			=	sfs_access,
	.create			=	sfs_create,
	.ftruncate		=	sfs_ftruncate,
	.utimens		=	sfs_utimens,
	.bmap			=	sfs_bmap,	// Required?? Similar to FIBMAP
	.ioctl			=	sfs_ioctl,
	.poll			=	sfs_poll,
	.write_buf		=	sfs_write_buf, 	// Similar to write
	.read_buf		=	sfs_read_buf,	// Similar to read
};

int
main(int argc, char *argv[])
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	int uid = getuid();
	int gid = getgid();
	int ret = -1;

#if 0
	// Check if root is mounting the file system.
	// If so, return error
	if (uid == 0 && gid == 0) {
		fprintf(stderr, "SFS: Mounting SFS by root opens security holes."
			" Please try mounting as a normal user\n");
		return -1;
	}
#endif

	// Initialize logging
	sfs_ctx = sfs_create_log_ctx();
	if (NULL == sfs_ctx) {
		fprintf(stderr, "%s: Log ctx creation failed. Logging disable",
			__FUNCTION__);
	} else {
		ret = sfs_log_init(sfs_ctx, sstack_log_level, "sfs");
		ASSERT((ret == 0), "Log initialization failed. Logging disabled",
			0, 0, 0);
	}
	ret = fuse_opt_parse(&args, NULL, sfs_opts, sfs_opt_proc);
	ASSERT((ret != -1), "Parsing arguments failed. Exiting ...", 0, 1, -1);
	mc = sstack_memcache_init("localhost", 1, sfs_ctx);

	ret = fuse_opt_add_arg(&args, "-obig_writes");
	ASSERT((ret != -1), "Enabling big writes failed.", 0, 0, 0);

	return fuse_main(args.argc, args.argv, &sfs_oper, NULL);
}

