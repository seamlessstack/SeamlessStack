
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


