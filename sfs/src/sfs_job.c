
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

#include <fuse.h>
#include <sfs.h>
#include <sfs_job.h>

/*
 * sfs_submit_job - Submit the job received from the sfs entry points
 *					to the job queue. These will be picked up by the
 *					dispatcher thread which sends to designated
 *					sfsds
 *
 * priority - priority of the job
 * job_lisy - job queue
 * job - job structure that encompasses the payload provided by the
 *		entry points.
 *
 * Returns 0 upon success and -1 on failure.
 *
 * NOTE:
 * Client waits on the condition variable. associated with the job.
 * Client will be woken up when the job finishes (whicever way)
 */

int
sfs_submit_job(int priority, sfs_job_queue_t *job_list, sfs_job_t *job)
{
	int ret = -1;
	// Parameter validation
	if (priority < HIGH_PRIORITY || priority > NUM_PRIORITY_MAX ||
					NULL == job_list || NULL == job) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);
		errno = EINVAL;

		return -1;
	}

	sfs_log(sfs_ctx, SFS_DEBUG, "%s:%d priority = %d \n", __FUNCTION__,
			__LINE__, priority);

	// Add the job to job queue
	ret = sfs_enqueue_job(priority, job_list, job);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to enqueue the job\n",
						__FUNCTION__);
		return -1;
	}

	sfs_log(sfs_ctx, SFS_ERR, "%s: success \n", __FUNCTION__);

	return 0;
}

/*
 * sfs_wait_for_completion - Waits for all the submitted jobs
 *						to be completed for the thread
 *
 * job_map - All the jobs associated with the thread. Should be non-NULL
 *
 * Returns 0 on success and -1 upon failure
 */

int
sfs_wait_for_completion(sstack_job_map_t *job_map)
{
	// Wait on condition variable
	pthread_mutex_lock(&job_map->wait_lock);
	pthread_cond_wait(&job_map->condition, &job_map->wait_lock);
	// Calling thread is waiting indefinitely for the completion status
	// of the submitted job.
	// This thread will be woken up when one of the worker threads condition
	// signals on jobs->wait_cond.
	// job->wait_mutex is still locked.
	pthread_mutex_unlock(&job_map->wait_lock);
#if 0
	// This code should be moved to worker thread
	ret = sfs_dequeue_job(priority, job_list, job);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failure in job handling \n",
						__FUNCTION__);
		return -1;
	}
#endif

	return (0);
}

/*
 * get_next_job_id - Return next job id
 *
 * Uses a bitmap to maintain active job ids.
 * Freed bits are not looked at until we wrap around.
 *
 * Returns job id in the range 0 .. (MAX_OUTSTANDING_JOBS - 1) on success
 * Returns -1 upon failure
 */

inline sstack_job_id_t
get_next_job_id(void)
{
	sstack_job_id_t job_id = -1;

	pthread_mutex_lock(&sfs_job_id_mutex);

	if (current_job_id >= MAX_OUTSTANDING_JOBS) {
		int i = 0;

		// Wrap around happened
		// Set current_job_id to the first undet bit in the bitmap
		for (i = 0; i < MAX_OUTSTANDING_JOBS; i++) {
			if (BITTEST(sstack_job_id_bitmap, i) == 0)
				break;
		}
		if (i == MAX_OUTSTANDING_JOBS) {
			sfs_log(sfs_ctx, SFS_ERR, "%s: All job slots currently occupied. "
							"Reject the job.\n", __FUNCTION__);

			return -1;
		}
		job_id = i;
		current_job_id = i;
		BITSET(sstack_job_id_bitmap,i); // Make this bit busy
		// Reinitalize current_job_id
		for (i = current_job_id; i < MAX_OUTSTANDING_JOBS; i++) {
			if (BITTEST(sstack_job_id_bitmap, i) == 0)
				break;
		}
		if (i < MAX_OUTSTANDING_JOBS) {
			current_job_id = i;
		} else {
			// No free slots available at this time for next job
			// Set current job id in such a way that next caller
			// will have to go through bitmap once again.
			current_job_id = MAX_OUTSTANDING_JOBS;
		}
	} else {
		// No wrap around
		job_id = current_job_id;
		current_job_id ++;
		BITSET(sstack_job_id_bitmap,job_id); // Make this bit busy
	}

	pthread_mutex_unlock(&sfs_job_id_mutex);

	return job_id;
}

/*
 * free_job_id - Free the job slot
 *
 * job_id - Bit to be cleared in the job bitmap
 *
 * Returns 0 on success and -1 on invalid job id.
 */
inline int
free_job_id(int job_id)
{
	// Validate
	if (job_id < 0 || job_id >= MAX_OUTSTANDING_JOBS) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid job id specified\n",
						__FUNCTION__);
		errno = EINVAL;
		return -1;
	}
	BITCLEAR(sstack_job_id_bitmap, job_id);

	return 0;
}

/*
 * sfs_job_init - Allocate job structure and initialize it
 *
 * Returns valid pointer on success and NULL on failure.
 */

inline sfs_job_t *
sfs_job_init(void)
{
	sfs_job_t *job = NULL;

	// Allocate memory
	job = (sfs_job_t *) calloc(sizeof(sfs_job_t), 1);
	if (NULL == job) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to allocate memory \n",
						__FUNCTION__);
		return NULL;
	}
	job->version = SFS_JOB_VERSION;
	job->id = get_next_job_id();
	job->priority = INVALID_PRIORITY;
	INIT_LIST_HEAD((bds_list_head_t) &job->wait_list);

	return job;
}

/*
 * sfs_job_list_init - Initialize global job queue list
 *
 * job_list - list of job queues - should be NULL
 *
 * Returns 0 on success and -1 on failure. Errno is untouched.
 */

inline int
sfs_job_list_init(sfs_job_queue_t **job_list)
{
	int i;
	int priority = HIGH_PRIORITY;
	sfs_job_queue_t *temp = NULL;

	*job_list = (sfs_job_queue_t *) malloc(sizeof(sfs_job_queue_t) *
					(NUM_PRIORITY_MAX + 1));
	if (NULL == *job_list) {
		sfs_log(sfs_ctx, SFS_CRIT, "%s: Failed to allocate memory for "
						"sfs job list. FATAL ERROR \n",  __FUNCTION__);
		return -1;
	}

	temp = *job_list;

	for (i = 0; i < NUM_PRIORITY_MAX; i++) {
		temp->priority = priority;
		INIT_LIST_HEAD((bds_list_head_t) &temp->list);
		pthread_spin_init(&temp->lock, PTHREAD_PROCESS_PRIVATE);
		priority ++;
		temp ++;
	}

	return 0;
}

/*
 * sfs_job_list_destroy - Destroy global job queue list
 */

inline int
sfs_job_queue_destroy(sfs_job_queue_t **job_list)
{
	int i = 0;
	sfs_job_queue_t *temp = NULL;

	// Parameter validation
	if (NULL == *job_list) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);
		return -1;
	}

	temp = *job_list;
	for (i = 0; i < NUM_PRIORITY_MAX; i++) {
		temp->priority = -1;
		// TODO
		// Free all the entries in the list here.
		INIT_LIST_HEAD((bds_list_head_t) &temp->list);
		pthread_spin_destroy(&temp->lock);
		temp ++;
	}

	free(*job_list);
	*job_list = NULL;

	return 0;
}



/*
 * sfs_enqueue_job - Enqueue new job in the queue
 *
 * priority - priority of the job
 * job - a non-NULL job structure to be queued
 *
 * Returns 0 on success and -1 on failure.
 */

inline int
sfs_enqueue_job(int priority, sfs_job_queue_t *job_list, sfs_job_t *job)
{
	// Parameter validation
	// Though the caller checks the params, this is done to catch stack
	// corruption issues. Can be thrown out later
	if (priority < HIGH_PRIORITY || priority > NUM_PRIORITY_MAX ||
				NULL == job_list || NULL == job) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);
		errno = EINVAL;

		return -1;
	}
	sfs_log(sfs_ctx, SFS_DEBUG, "%s: priority = %d\n",
			__FUNCTION__, priority);
	job_list += priority; // Move to the specified priority list
	// Add to tail to maintain FIFO semantics
	pthread_spin_lock(&job_list->lock);
	bds_list_add_tail((bds_list_head_t) &job->wait_list,
					(bds_list_head_t) &job_list->list);
	pthread_spin_unlock(&job_list->lock);

	return 0;
}

/*
 * sfs_dequeue_job - Remove the specified job from the job list
 *
 * priority - priority of the job. This is to minimize number of lists
 *			  to look at
 * job_list - global job list
 * job - Job to be removed
 *
 * Returns 0 on success and -1 on failure.
 */

inline int
sfs_dequeue_job(int priority, sfs_job_queue_t *job_list, sfs_job_t *job)
{
	bds_list_head_t head = NULL;
	sfs_job_t *temp = NULL;
	int found = -1;

	// Parameter validation
	if (priority < HIGH_PRIORITY ||  priority > NUM_PRIORITY_MAX ||
					 NULL == job_list || NULL == job) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);
		errno = EINVAL;

		return -1;
	}
	job_list += priority; // Move to the specified priority list
	// Delete the job from job list
	pthread_spin_lock(&job_list->lock);
	if (list_empty((bds_list_head_t) &job_list->list)) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Empty job list at priority %d . "
						"This could indicate list corruption \n",
						__FUNCTION__, priority);
		pthread_spin_unlock(&job_list->lock);
		return -1;
	}
	pthread_spin_unlock(&job_list->lock);
	// Walk thorough the list and delete the element that matches
	// the job id
	head = (bds_list_head_t) &job_list->list;
	pthread_spin_lock(&job_list->lock);
	list_for_each_entry(temp, head, wait_list) {
		if (temp->id == job->id) {
			bds_list_del((bds_list_head_t) &temp->wait_list);
			found = 1;
			break;
		}
	}
	pthread_spin_unlock(&job_list->lock);
	if (found != 1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Job not found in job list. "
						"Possible job list corruption for priority %d \n",
						__FUNCTION__, priority);
		return -1;
	}

	return 0;
}

/*
 * create_job_map - Allocates memory for job_map structure and initializes
 *					all the fields
 *
 * Returns initialized job_map structure upon success and NULL upon failure
 */

inline sstack_job_map_t *
create_job_map(void)
{
	sstack_job_map_t *job_map = NULL;

	job_map = malloc(sizeof(sstack_job_map_t));
	if (NULL == job_map)
		return NULL;

	// Initialize job_map
	job_map->thread_id = pthread_self();
	job_map->num_jobs = 0;
	pthread_spin_init(&job_map->lock, PTHREAD_PROCESS_PRIVATE);
	job_map->job_ids = NULL;
	job_map->job_status = NULL;
	pthread_mutex_init(&job_map->wait_lock, NULL);
	pthread_cond_init(&job_map->condition, NULL);

	return job_map;
}

/*
 * free_job_map - Free all the resources used by job_map
 */

inline void
free_job_map(sstack_job_map_t *job_map)
{
	if (job_map != NULL) {
		pthread_spin_destroy(&job_map->lock);
		pthread_mutex_destroy(&job_map->wait_lock);
		pthread_cond_destroy(&job_map->condition);
		free(job_map);
		job_map = NULL;
	}
}
