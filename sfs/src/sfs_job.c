
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

	// Add the job to job queue
	ret = sfs_enqueue_job(priority, job_list, job);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to enqueue the job\n",
						__FUNCTION__);
		return -1;
	}
	// Wait on condition variable
	pthread_mutex_lock(&job->wait_mutex);
	pthread_cond_wait(&job->wait_cond, &job->wait_mutex);
	// Calling thread is waiting indefinitely for the completion status
	// of the submitted job.
	// This thread will be woken up when one of the worker threads condition
	// signals on jobs->wait_cond.
	// job->wait_mutex is still locked.
	pthread_mutex_unlock(&job->wait_mutex);
	ret = sfs_dequeue_job(priority, job_list, job);
	if (ret != 0) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failure in job handling \n",
						__FUNCTION__);
		return -1;
	}

	// TODO
	// Send the status back to the application

	return 0;
}


