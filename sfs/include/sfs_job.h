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

#ifndef __SFS_JOB_H__
#define __SFS_JOB_H__

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sstack_log.h>
#include <sstack_jobs.h>
#include <sstack_types.h>
#include <sstack_bitops.h>
#include <sstack_md.h>
#include <sfs_internal.h>

extern sstack_job_id_t  current_job_id; // Points to next job id to be returned
extern pthread_mutex_t sfs_job_id_mutex;
extern sstack_bitmap_t *sstack_job_id_bitmap;

inline sstack_job_id_t get_next_job_id(void);
inline int free_job_id(int );
inline sfs_job_t * sfs_job_init(void);
inline int sfs_job_list_init(sfs_job_queue_t **);
inline int sfs_job_queue_destroy(sfs_job_queue_t **);
inline int sfs_enqueue_job(int , sfs_job_queue_t *, sfs_job_t *);
inline int sfs_dequeue_job(int , sfs_job_queue_t *, sfs_job_t *);
extern int sfs_submit_job(int , sfs_job_queue_t *, sfs_job_t *);

/*
 * This is the structure returned by IDP for reading/writing
 */
typedef struct sfsd_list {
	int num_sfsds;
	sfsd_t *sfsds;
} sfsd_list_t;

/*
 * TODO
 * There needs to be an association for each pthread id to set of jobs 
 * submitted by it. This is because each read/write request can split
 * into multiple jobs and the thread will sleep on a condition variable.
 * When a job finishes, worker thread goes and fetches pthread id based
 * structure(defined below) and marks the job complete/failed. In the same
 * function, all the jobs are checked for completion. If all jobs submitted
 * by a thread are complete, signal the condition variable.
 * Use RB-tree for storing these structures:
 * pthread is to jobs mapping
 * job to pthread id mapping
 */
typedef struct job_map {
	pthread_t thread_id;
	int	err_no;
	int num_jobs;
	int num_jobs_left; /* used to check for signalling the thread
						* waiting on the condition variable */
	pthread_spinlock_t lock;
	sstack_job_id_t *job_ids;
	sstack_job_status_t *job_status;
	int	num_clients;
	sstack_job_status_t op_status[MAX_SFSD_CLIENTS];
									/* Use the same job status enum
									 * but this specifies the entire
									 * operation status per client 
									 * basis */
	pthread_mutex_t wait_lock;
	pthread_cond_t condition;
} sstack_job_map_t;

inline sstack_job_map_t * create_job_map(void);
inline void free_job_map(sstack_job_map_t *);
extern int sfs_wait_for_completion(sstack_job_map_t *);

// TODO
// Following functions need to be implemented

extern sfsd_list_t * get_sfsd_list(sstack_inode_t *);
/*
 * Reverse map function to get pthread is given the job id
 */
extern pthread_t get_thread_id(sstack_job_id_t );

extern sfsd_list_t * sfs_idp_get_sfsd_list(sstack_inode_t *,
				sstack_sfsd_pool_t *, log_ctx_t *);
#endif // __SFS_JOB_H__
