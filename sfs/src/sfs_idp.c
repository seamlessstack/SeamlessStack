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
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <sfs.h>
#include <sstack_jobs.h>
#include <sstack_md.h>
#include <policy.h>
#include <bds_types.h>
#include <bds_list.h>
#include <sstack_log.h>
#include <sfs_internal.h>
#include <sfs_job.h>

#define SFS_IDP_NHOP_WGT	1
#define SFS_IDP_STORAGE_WGT	2

struct idp_list
{
	sfsd_t 			*sfsd;
	uint32_t		weight;
	bds_int_list_t	list;
};

struct idp_prio_list
{
	struct idp_list qos_high_list;
	struct idp_list qos_med_list;
	struct idp_list qos_low_list;
};

struct idp_prio_list prio_pool;
struct idp_list *sfsd_sorted;
uint32_t sfsd_index = 0;

static void
sfs_idp_calculate_and_add(sfsd_t *sfsd)
{
	struct idp_list *sfsd_entry;
	struct idp_list *s;
	uint32_t	stor_wgt = 0;

	if ((sfsd_entry = (struct idp_list *)calloc(sizeof(struct idp_list), 1))
						== NULL) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Allocation failed for sfsd_entry\n",
					__FUNCTION__);
		return;
	}


	sfsd_entry->weight += sfsd->sfs_pool_wgt * SFS_IDP_NHOP_WGT;
	//TBD: call into mongo db to get stor_wgt
	sfsd_entry->weight += stor_wgt * SFS_IDP_STORAGE_WGT;

	if (list_empty((bds_list_head_t) &sfsd_sorted->list)) {
		bds_list_add((bds_list_head_t) &sfsd_entry->list,
							(bds_list_head_t) &sfsd_sorted->list);
		sfsd_index++;
	} else {
		list_for_each_entry(s, (bds_list_head_t) &sfsd_sorted->list, list) {
			if (sfsd_entry->weight <= s->weight) {
				break;
			}
		}
		s->sfsd = sfsd;
		bds_list_add_tail((bds_list_head_t) &sfsd_entry->list,
								(bds_list_head_t) &s->list);
		sfsd_index++;
	}
}

static void
sfs_idp_traverse_sfs_pool(sstack_sfsd_pool_t *pools)
{
	int		i = 0;
	sfsd_t	*s;
	bds_list_head_t head;

	// Parameter validation
	if (NULL == pools) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameter specified\n",
						__FUNCTION__);
		errno = EINVAL;
		return;
	}

	for (i = 0; i < MAX_SFSD_POOLS; i++) {
		head = (bds_list_head_t) &(pools->list);
		list_for_each_entry(s, head, list) {
			sfs_idp_calculate_and_add(s);
		}
		pools++;
	}
}

static void
sfs_idp_fill_qos_prio_pools(void)
{
	struct idp_list *s;
	int     iter = 0;

	/* first discard the DR(the last sfsd id before segregating into
	 * priority pools
	 */
	sfsd_index--;

	INIT_LIST_HEAD((bds_list_head_t) &prio_pool.qos_high_list);
	INIT_LIST_HEAD((bds_list_head_t) &prio_pool.qos_med_list);
	INIT_LIST_HEAD((bds_list_head_t) &prio_pool.qos_low_list);

	list_for_each_entry(s, (bds_list_head_t) &sfsd_sorted->list, list) {
		if (iter <= sfsd_index/3) {
			bds_list_add_tail((bds_list_head_t) &(prio_pool.qos_high_list.list),
								(bds_list_head_t) &s->list);
		}

		if ((iter >= sfsd_index/3) && (iter <= (2 * sfsd_index)/3)) {
			bds_list_add_tail((bds_list_head_t) &(prio_pool.qos_med_list.list),
								(bds_list_head_t) &s->list);
		}

		if (iter >= (2 * sfsd_index)/3) {
			if (list_entry(s->list.next, struct idp_list, list) == sfsd_sorted)
				continue;
			bds_list_add_tail((bds_list_head_t) &(prio_pool.qos_low_list.list),
								(bds_list_head_t) &s->list);
		}
	}
}

/*
 * sfs_idp_get_sfsd_list - Get the list of target sfsds for the file
 *							by policy lookup
 *
 * inode - Inode structure of the file
 * pools - List of sfsds handled by current sfs
 * ctx - Log context
 *
 * Returns list of sfsds upon success and NULL upon failure.
 * Caller should free up the allocated memory.
 */

sfsd_list_t *
sfs_idp_get_sfsd_list(sstack_inode_t *inode, sstack_sfsd_pool_t *pools,
						log_ctx_t *ctx)
{
	policy_entry_t	*pe;
	uint8_t			qos;
	uint8_t			num_replicas = 0;
	sfsd_list_t		*sfsd_list = NULL;
	int				i = 1;
	struct idp_list *s;
	sfsd_t			*sfsd;

	// Parameter validation
	if (NULL == inode || NULL == pools) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);
		errno = EINVAL;
		return NULL;
	}

	sfsd_index = 0;
	pe = get_policy(inode->i_name);
	qos = pe->pe_attr.a_qoslevel;
	num_replicas = pe->pe_attr.a_numreplicas;

	if (pe->pe_attr.a_enable_dr) {
		num_replicas++;
	}

	sfsd_list = (sfsd_list_t *) calloc(sizeof(sfsd_list_t), 1);
	if (NULL == sfsd_list) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Unable to allocate memory for "
						"sfsd_list. \n", __FUNCTION__);
		return NULL;
	}

	// Allocate memory for sfsds pointers
	// One extra to account for DR
	sfsd_list->sfsds = (sfsd_t *) calloc((sizeof(uintptr_t) *
							(num_replicas + 1)), 1);
	if (NULL == sfsd_list->sfsds) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to allocate memory for "
						"sfsd_list->sfsds \n", __FUNCTION__);
		free(sfsd_list);
		return NULL;
	}

	if ((sfsd_sorted = (struct idp_list *)calloc(sizeof(struct idp_list), 1))
						== NULL) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Allocation failed for sfsd_sorted\n",
					__FUNCTION__);
		return (NULL);
	}
	INIT_LIST_HEAD((bds_list_head_t) &sfsd_sorted->list);

	sfsd_list->num_sfsds = num_replicas;
	sfs_idp_traverse_sfs_pool(pools);
	sfs_idp_fill_qos_prio_pools();

	switch (qos) {
		case (QOS_HIGH):
			list_iter_entry(s,
					(bds_list_head_t) &(prio_pool.qos_high_list.list), list) {
				if (s == &(prio_pool.qos_high_list))
					continue;
				sfsd = (sfsd_list->sfsds + i);
				memcpy(sfsd, s->sfsd, sizeof(uintptr_t));
				i++;
				//sfsd_list->sfsds[i++] = s->sfsd;
				if (i == (num_replicas + 1))
					break;
			}
			break;

		case (QOS_MEDIUM):
			list_iter_entry(s,
					(bds_list_head_t) &(prio_pool.qos_med_list.list), list) {
				if (s == &(prio_pool.qos_med_list))
					continue;

				sfsd = (sfsd_list->sfsds + i);
				memcpy(sfsd, s->sfsd, sizeof(uintptr_t));
				i++;
				if (i == (num_replicas + 1))
					break;
			}
			break;

		case (QOS_LOW):
			list_iter_entry(s,
					(bds_list_head_t) &(prio_pool.qos_med_list.list), list) {
				if (s == &(prio_pool.qos_med_list))
					continue;

				sfsd = (sfsd_list->sfsds + i);
				memcpy(sfsd, s->sfsd, sizeof(uintptr_t));
				i++;
				if (i == (num_replicas + 1))
					break;
			}
			break;

		default:
			break;
	}

	if (pe->pe_attr.a_enable_dr) {
		s = list_entry(sfsd_sorted->list.prev, struct idp_list, list);
		sfsd = (sfsd_list->sfsds + num_replicas);
		memcpy(sfsd, s->sfsd, sizeof(uintptr_t));
		i++;
		sfsd_list->num_sfsds ++;
	}

	/* Free sfsd_sorted list */
	list_iter_entry(s, (bds_list_head_t) &sfsd_sorted->list, list) {
		if (list_entry(s->list.prev, struct idp_list, list) == sfsd_sorted)
			continue;
		free(s->list.prev);
		if (s == sfsd_sorted)
			break;
	}
	free(sfsd_sorted);

	return (sfsd_list);
}
