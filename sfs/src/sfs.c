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
#include <sstack_md.h>
#include <sstack_db.h>
#include <sstack_jobs.h>
#include <sstack_log.h>
#include <sstack_helper.h>
#include <sstack_version.h>
#include <sstack_bitops.h>
#include <sstack_cache_api.h>
#include <policy.h>
#include <sfs.h>
#include <sfs_entry.h>
#include <mongo_db.h>
/* Macros */
#define MAX_INODE_LEN 40 // Maximum len of uint64_t is 39

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

/* Forward declarations */
static int sfs_store_branch(char *branch);
static int sfs_remove_branch(char *branch);


/* Functions */

static int
add_inodes(const char *path)
{
	char *buffer = NULL;
	sstack_extent_t attr;
	sstack_inode_t *inode;
	policy_entry_t *policy = NULL;
	struct stat status;
	char inode_str[MAX_INODE_LEN] = { '\0' };
	int ret = -1;
	extent_path_t *ep = NULL;

    sfs_log(sfs_ctx, SFS_DEBUG, "%s: path = %s\n", __FUNCTION__, path);
    if (lstat(path, &status) == -1) {
		sfs_log(sfs_ctx, SFS_ERR,
			"%s: stat failed on %s with error %d\n",
			__FUNCTION__, path, errno);
		return -errno;
    }

	buffer = calloc((sizeof(sstack_inode_t) + sizeof(sstack_extent_t)), 1);
	if (NULL == buffer) {
		// TBD
		sfs_log(sfs_ctx,SFS_CRIT,
				"Out of memory at line %d function %s \n",
				__LINE__, __FUNCTION__);
		return -1;
	}

	// Use buffer as (inode + extent)
	inode = (sstack_inode_t *)buffer;
	// Populate inode structure
	inode->i_num = get_free_inode();
	strcpy(inode->i_name, path);
	inode->i_uid = status.st_uid;
	inode->i_gid = status.st_gid;
	inode->i_mode = status.st_mode;
	switch (status.st_mode & S_IFMT) {
		case S_IFDIR:
			inode->i_type = DIRECTORY;
			break;
		case S_IFREG:
			inode->i_type = REGFILE;
			break;
		case S_IFLNK:
			inode->i_type = SYMLINK;
			break;
		case S_IFBLK:
			inode->i_type = BLOCKDEV;
			break;
		case S_IFCHR:
			inode->i_type = CHARDEV;
			break;
		case S_IFIFO:
			inode->i_type = FIFO;
			break;
		case S_IFSOCK:
			inode->i_type = SOCKET_TYPE;
			break;
		default:
			inode->i_type = UNKNOWN;
			break;
	}

	// Make sure we are not looping
	// TBD

	memcpy(&inode->i_atime, &status.st_atime, sizeof(struct timespec));
	memcpy(&inode->i_ctime, &status.st_ctime, sizeof(struct timespec));
	memcpy(&inode->i_mtime, &status.st_mtime, sizeof(struct timespec));
	inode->i_size = status.st_size;
	inode->i_ondisksize = (status.st_blocks * 512);
	inode->i_numreplicas = 1; // For now, single copy
	// Since the file already exists, done't split it now. Split it when
	// next write arrives
	inode->i_numextents = 1;
	inode->i_links = status.st_nlink;
	sfs_log(sfs_ctx, SFS_INFO,
		"%s: nlinks for %s are %d\n", __FUNCTION__, path,
		inode->i_links);
	// Populate size of each extent
	policy = get_policy(path);
	if (NULL == policy) {
		sfs_log(sfs_ctx, SFS_INFO,
			"%s: No policies specified. Default policy applied\n",
			__FUNCTION__);
		inode->i_policy.pe_attr.a_qoslevel = QOS_LOW;
		inode->i_policy.pe_attr.a_ishidden = 0;
	} else {
		// Got the policy
		sfs_log(sfs_ctx, SFS_INFO,
			"%s: Got policy for file %s\n", __FUNCTION__, path);
		sfs_log(sfs_ctx, SFS_INFO, "%s: ver %s qoslevel %d hidden %d \n",
			__FUNCTION__, path, policy->pe_attr.ver,
			policy->pe_attr.a_qoslevel,
			policy->pe_attr.a_ishidden);
		memcpy(&inode->i_policy, policy, sizeof(policy_entry_t));
	}
	// Populate the extent
	memset(&attr, 0, sizeof(sstack_extent_t));
	attr.e_realsize = status.st_size;
	if (inode->i_type == REGFILE) {
		attr.e_cksum = sstack_checksum(sfs_ctx, path); // CRC32
		sfs_log(sfs_ctx, SFS_INFO, "%s: checksum of %s is %lu \n",
			__FUNCTION__, path, attr.e_cksum);
	} else
		attr.e_cksum = 0; // Place holder
	ep = attr.e_path;
	strcpy(ep->extent_path, path);
	memcpy((buffer+sizeof(sstack_inode_t)), &attr, sizeof(sstack_extent_t));

	// Now inode is ready to be placed in DB
	// Put it in DB
	sprintf(inode_str, "%lld", inode->i_num);
	if (db->db_ops.db_insert && (db->db_ops.db_insert(inode_str, buffer,
		(sizeof(sstack_inode_t) + sizeof(sstack_extent_t)),
		INODE_TYPE, sfs_ctx) != 1)) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to add inode to db . \n",
			__FUNCTION__);
		free(buffer);
		/* Though the operation failed. nothing much can be done. */
		return 0;
	}

	// Insert into reverse lookup db
	// TBD
	ret = sstack_cache_store(mc, path, inode_str, strlen(inode_str) + 1);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to store object into memcached."
				" Key = %s value = %s \n", __FUNCTION__, path, inode_str);
		return 0;
	}

	free(buffer);

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

static void *
handle_requests(void * arg)
{
	int sockfd, new_fd;
	socklen_t sin_size;
	int yes=1;
	struct sigaction sa;
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage client_addr;
	int rv = -1;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; // use my IP

	if ((rv = getaddrinfo(NULL, CLI_PORT, &hints, &servinfo)) != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: getaddrinfo: %s\n",
			__FUNCTION__, gai_strerror(rv));
		return NULL;
	}

	// loop through all the results and bind to the first we can

	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
			sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to create socket \n",
				__FUNCTION__);
			continue;
		}

		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			sfs_log(sfs_ctx, SFS_ERR, "%s: setsockopt failed \n", __FUNCTION__);
			return NULL;
		}

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			continue;
		}
		break;
	}

	if (NULL == p) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to bind \n", __FUNCTION__);
		return NULL;
	}
	freeaddrinfo(servinfo);

	if (listen(sockfd, LISTEN_QUEUE_SIZE) == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Listen failed \n", __FUNCTION__);
		close(sockfd);
		return NULL;
	}

	sa.sa_handler = sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: sigaction failed\n", __FUNCTION__);
		close(sockfd);
		return NULL;
	}

	// Start handling connections
	while(1) {
		int nbytes = -1;
		sfs_client_request_t req ;
		char buf[sizeof(sfs_client_request_t)];
		char addr[INET6_ADDRSTRLEN];

		sin_size = sizeof(struct sockaddr_storage);
		new_fd = accept(sockfd, (struct sockaddr *) &client_addr, &sin_size);
		if (new_fd == -1) {
			sfs_log(sfs_ctx, SFS_ERR, "%s: Accept failed\n", __FUNCTION__);
			continue;
		}

		// Connection is from addr
		// Store it to check whether request is from local box or the remote
		inet_ntop(client_addr.ss_family,
			get_in_addr((struct sockaddr *)&client_addr), addr,
			INET6_ADDRSTRLEN);
		// Not a concurrent server.
		// No ned for a concurrent server as add/del requests should be rare
		// To convert to concurrent server, create a new thread call handle
		// from that thread.

		sfs_log(sfs_ctx, SFS_INFO, "%s: Got a connection\n", __FUNCTION__);
		nbytes = recv(new_fd, buf, sizeof(sfs_client_request_t), 0);
		if (nbytes < sizeof(sfs_client_request_t)) {
			sfs_log(sfs_ctx, SFS_ERR, "%s: Incomplete request read \n",
				__FUNCTION__);
			close(sockfd);
			return NULL;
		}

		// Deserialize
		deserialize(&req, buf);

		// Get local IP addresses and match it with incoming connection's IP
		// address. If not a local IP address, use sshfs to mount remote
		// directory to a local directory. Update req with the local mount
		// point
//		if (req.u1.req_type == ADD_BRANCH)
//			update_request(addr, &req);
		handle(&req);
		close(new_fd);
	}

	return NULL;
}


static void *
sfs_init(struct fuse_conn_info *conn)
{
	pthread_t thread;
	pthread_attr_t attr;
	int ret = -1;
	int chunk_index = 0;

	// Create a thread t handle client requests
	if(pthread_attr_init(&attr) == 0) {
		pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
		pthread_attr_setstacksize(&attr, 65536);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
		ret = pthread_create(&thread, &attr, handle_requests, NULL);
	}

	ASSERT((ret == 0),"Unable to create thread to handle cli requests",
			0, 0, 0);
	(void) conn->max_readahead;
	// Create db instance
	db = create_db();
	ASSERT((db != NULL), "Unable to create db. FATAL ERROR", 0, 1, NULL);
	db_register(db, mongo_db_init, mongo_db_open, mongo_db_close,
		mongo_db_insert, mongo_db_iterate, mongo_db_get,
		mongo_db_seekread, mongo_db_update, mongo_db_delete,
		mongo_db_cleanup, sfs_ctx);
	if (db->db_ops.db_init(sfs_ctx) != 0) {
		sfs_log(sfs_ctx, SFS_CRIT, "%s: DB init faled with error %d\n",
			__FUNCTION__, errno);
		return NULL;
	}
	if (db->db_ops.db_open(sfs_ctx) != 0) {
		sfs_log(sfs_ctx, SFS_CRIT, "%s: DB open faled with error %d\n",
			__FUNCTION__, errno);
		return NULL;
	}

	// Other init()s go here

	// TBD
	// Need to create one more collection for reverse lookup table
	// This is to get inode number given pathname

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
	ASSERT((sfs_chunks != NULL), "Memory alocation failed", 0, 1, 0);
	if (NULL == temp)
		exit(-1);
	sfs_chunks = (sfs_chunk_entry_t *) temp;
	memset((void *) (sfs_chunks + nchunks), '\0', sizeof(sfs_chunk_entry_t));
	ptr = (char **) &branch;
	res = strsep(ptr, ",");
	if (!res)
		return 0;
	strncpy(sfs_chunks[nchunks].ipaddr, res, IPV6_ADDR_LEN - 1);
	res = strsep(ptr, ",");
	if (!res)
		return 0;
	sfs_chunks[nchunks].chunk_path = strndup(res, PATH_MAX - 1);
	sfs_chunks[nchunks].chunk_pathlen = strlen(res) + 1;
	sfs_chunks[nchunks].rw = 0;
	sfs_chunks[nchunks].weight = DEFAULT_WEIGHT;

	res = strsep(ptr, ",");
	if (res) {
		if (strcasecmp(res, "rw") == 0) {
			sfs_chunks[nchunks].rw = 1;
		}
	}

	res = strsep(ptr, ",");
	if (res) {
		uint32_t weight = atoi(res);

		if (weight > MINIMUM_WEIGHT && weight < MAXIMUM_WEIGHT)
			sfs_chunks[nchunks].weight = weight;
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

		nbranches += sfs_store_branch(branch);
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
	.flush			=	sfs_flush,
	.release		=	sfs_release,
	.fsync			=	sfs_fsync,
	.setxattr		=	sfs_setxattr,
	.getxattr		=	sfs_getxattr,
	.listxattr		=	sfs_listxattr,
	.removexattr		=	sfs_removexattr,
	.opendir		=	sfs_opendir,
	.releasedir		=	sfs_releasedir,
	.fsyncdir		=	sfs_fsyncdir,
	.init			=	sfs_init,
	.destroy		=	sfs_destroy,
	.access			=	sfs_access,
	.create			=	sfs_create,
	.ftruncate		=	sfs_ftruncate,
	.fgetattr		=	sfs_fgetattr,
	.lock			=	sfs_lock,
	.utimens		=	sfs_utimens,
	.bmap			=	sfs_bmap,	// Required?? Similar to FIBMAP
	.ioctl			=	sfs_ioctl,
	.poll			=	sfs_poll,
	.write_buf		=	sfs_write_buf, 	// Similar to write
	.read_buf		=	sfs_read_buf,	// Similar to read
	.flock			=	sfs_flock,
};

int
main(int argc, char *argv[])
{
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
	int uid = getuid();
	int gid = getgid();
	int ret = -1;

	// Check if root is mounting the file system.
	// If so, return error
	if (uid == 0 && gid == 0) {
		fprintf(stderr, "SFS: Mounting SFS by root opens security holes."
			" Please try mounting as a normal user\n");
		return -1;
	}

	ret = fuse_opt_parse(&args, NULL, sfs_opts, sfs_opt_proc);
	ASSERT((ret != -1), "Parsing arguments failed. Exiting ...", 0, 1, -1);
	// Initialize logging
	sfs_ctx = sfs_create_log_ctx();
	if (NULL == sfs_ctx) {
		fprintf(stderr, "%s: Log ctx creation failed. Logging disable",
			__FUNCTION__);
	} else {
		ret = sfs_log_init(sfs_ctx, sstack_log_level, "sfs");
		ASSERT((ret != 0), "Log initialization failed. Logging disabled",
			0, 0, 0);
	}
	mc = sstack_cache_init("localhost", 1);

	ret = fuse_opt_add_arg(&args, "-obig_writes");
	ASSERT((ret != -1), "Enabling big writes failed.", 0, 0, 0);

	return fuse_main(args.argc, args.argv, &sfs_oper, NULL);
}

