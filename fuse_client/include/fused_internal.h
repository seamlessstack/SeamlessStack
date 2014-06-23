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

#ifndef __FUSED_INTERNAL_H__
#define __FUSED_INTERNAL_H__

#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <sstack_jobs.h>
#include <bds_list.h>
#include <sstack_log.h>
#include <sstack_md.h>
#include <sstack_helper.h>

extern log_ctx_t *fused_ctx;
extern sfs_job_queue_t *jobs;
extern sfs_job_queue_t *pending_jobs;


inline sstack_sfsd_pool_t * sstack_sfsd_pool_init(void);
inline int sstack_sfsd_pool_destroy(sstack_sfsd_pool_t *);
inline int sstack_sfsd_add(uint32_t , sstack_sfsd_pool_t *, sfsd_t *);
inline int sstack_sfsd_remove(uint32_t , sstack_sfsd_pool_t *, sfsd_t *);

#endif // __FUSED_INTERNAL_H__
