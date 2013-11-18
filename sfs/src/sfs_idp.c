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
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sfs.h>
#include <sstack_md.h>
#include <policy.h>
#include <bds_list.h>

#define SFS_IDP_NHOP_WGT	1
#define SFS_IDP_STORAGE_WGT	2

struct idp_list
{
	uint32_t		sfsd_id;
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
uint32_t index = 0;

static void
sfs_idp_calculate_and_add(sfsd_t *sfsd)
{
	struct idp_list *sfsd_entry;
	struct idp_list *s;
	uint32_t	stor_wgt = 0;

	if ((sfsd_entry = (struct idp_list *)calloc(sizeof(struct idp_list), 1))
						== NULL) {
		sfs_log(ctx, SFS_ERR, "%s: Allocation failed for sfsd_entry\n",
					__FUNCTION__);
		return (-errno);
	}	
	sfsd_entry->weight += sfsd->sfs_pool_wgt * SFS_IDP_NHOP_WGT;
	//TBD: call into mongo db to get stor_wgt
	sfsd_entry->weight += stor_wgt * SFS_IDP_STORAGE_WGT; 

	if (list_empty((bds_list_head_t) &sfsd_sorted->list)) {
		bds_list_add((bds_list_head_t) &sfsd_entry->list, 
							(bds_list_head_t) &sfsd_sorted->list);
		index++;
	} else {
		list_for_each_entry(s, (bds_list_head_t) &sfsd_sorted->list, list) {
			if (sfsd_entry->weight <= s->weight) {
				break;
			}
		}
		bds_list_add_tail((bds_list_head_t) &sfsd_entry->list,
								(bds_list_head_t) &s->list);
		index++;
	}	
}

static void
sfs_idp_traverse_sfs_pool(sstack_sfsd_pool_t *pools)
{
	int		i = 0, j;
	sfsd_t	*s;
	bds_list_head_t head;

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
	index--;
	
	INIT_LIST_HEAD((bds_list_head_t) &prio_pool->qos_high_list);
	INIT_LIST_HEAD((bds_list_head_t) &prio_pool->qos_med_list);
	INIT_LIST_HEAD((bds_list_head_t) &prio_pool->qos_low_list);

	list_for_each_entry(s, (bds_list_head_t) &sfsd_sorted->list, list) {
		if (iter <= index/3) {
			bds_list_add_tail((bds_list_head_t) &(prio_pool.qos_high_list.list),
								(bds_list_head_t) &s->list);
		}

		if ((iter >= index/3) && (iter <= (2 * index)/3)) {
			bds_list_add_tail((bds_list_head_t) &(prio_pool.qos_med_list.list),
								(bds_list_head_t) &s->list);
		}			

		if (iter >= (2 * index)/3) {
			if (list_entry(s->list.next, struct idp_list, list) == sfsd_sorted)
				continue;
			bds_list_add_tail((bds_list_head_t) &(prio_pool.qos_low_list.list),
								(bds_list_head_t) &s->list);
		}
	}	
}

int *
sfs_idp_get_sfsd_list(sstack_inode_t *inode, sstack_sfsd_pool_t *pools,
						log_ctx_t *ctx)
{
	policy_entry_t	*pe;
	uint8_t			qos;
	uint8_t			num_replicas = 0;
	uint32_t		*sfsd_list = NULL;
	int				i = 1;
	struct idp_list *s;

	index = 0;
	pe = get_policy(inode->i_name);
	qos = pe->pe_attr.a_qoslevel;
	num_replicas = pe->pe_attr.a_numreplicas;
	
	if (pe->pe_attr.a_enable_dr) {
		num_replicas++;
	}

	if (!(sfsd_list = calloc(sizeof(uint32_t), (num_replicas + 1)))) {
		sfs_log(ctx, SFS_ERR, "%s: Allocation failed for sfsd_list\n",
					__FUNCTION__);
		return (-errno);
	}
	
	if ((sfsd_sorted = (struct idp_list *)calloc(sizeof(struct idp_list), 1))
						== NULL) {
		sfs_log(ctx, SFS_ERR, "%s: Allocation failed for sfsd_sorted\n",
					__FUNCTION__);
		return (-errno);
	}	
	INIT_LIST_HEAD((bds_list_head_t) &sfsd_sorted->list);

	sfsd_list[0] = num_replicas;
	sfs_idp_traverse_sfs_pool(pools);
	sfs_idp_fill_qos_prio_pools();

	switch (qos) {
		case (QOS_HIGH): 
			list_iter_entry(s, 
					(bds_list_head_t) &(prio_pool.qos_high_list.list), list) {
				if (s == &(prio_pool.qos_high_list))  
					continue;
				
				sfsd_list[i++] = s->sfsd_id;
				if (i == (num_replicas + 1)) 
					break;
			}
			break;
		
		case (QOS_MEDIUM): 
			list_iter_entry(s, 
					(bds_list_head_t) &(prio_pool.qos_med_list.list), list) {
				if (s == &(prio_pool.qos_med_list))  
					continue;
				
				sfsd_list[i++] = s->sfsd_id;
				if (i == (num_replicas + 1))
					break;
			}
			break;
			
		case (QOS_LOW):
			list_iter_entry(s, 
					(bds_list_head_t) &(prio_pool.qos_med_list.list), list) {
				if (s == &(prio_pool.qos_med_list))  
					continue;
			
				sfsd_list[i++] = s->sfsd_id;
				if (i == (num_replicas + 1))
					break;
			}
			break;
		
		default:
			break;
	}
	
	if (pe->pe_attr.a_enable_dr) {
		s = list_entry(sfsd_sorted->list.prev, struct idp_list, list);
		sfsd_list[num_replicas] = s->sfsd_id;
	}

	/* Free sfsd_sorted list */
	list_iter_entry(s, (bds_list_head_t) &sfsd_sorted->list, list) {
		if (list_entry(s->prev, struct idp_list, list) == sfsd_sorted)
			continue;
		free(s->prev);
		if (s == sfsd_sorted)
			break;
	}	
	free(sfsd_sorted);	
	
	return (sfsd_list);
}	
