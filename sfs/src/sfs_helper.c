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

#include <sfs.h>
#include <sfs_internal.h>

// All helper functions pertaining to SFS go here

/*
 * get_local_ip - Return IP address of the interface specified.
 *
 * interface - string representing the interface. Should be non-NULL
 * intf_addr - Ouput parameter. Will be allocated here
 * type -  1 for IPv4 and 0 for IPv6
 *
 * Returns 0 on success. Returns -1 on error.
 */

char *
get_local_ip(char *interface, int type, log_ctx_t *ctx)
{
	int fd;
	struct ifreq ifr;
	int family = -1;
	int len = 0;
	char *intf_addr = NULL;

	// Parameter validation
	if (NULL == interface || type < 0 || type > IPv4) {
		// Invalid parameters
		errno = EINVAL;
		sfs_log(ctx, SFS_ERR, "%s: Invalid parameters specified \n",
						__FUNCTION__);

		return NULL;
	}


	if (type == IPv6) {
		len = IPV6_ADDR_LEN;
		family = AF_INET6;
	} else {
		len = IPV4_ADDR_LEN;
		family = AF_INET;
	}

	intf_addr = (char *) malloc(len);
	if (NULL == intf_addr) {
		sfs_log(ctx, SFS_ERR, "%s:%d Failed to allocate \n",
						__FUNCTION__, __LINE__);
		return NULL;
	}

	fd = socket(family, SOCK_DGRAM, 0);
	if (fd == -1) {
		// Socket creation failed
		sfs_log(ctx, SFS_ERR, "%s:%d socket creation failed\n",
						__FUNCTION__, __LINE__);

		return NULL;
	}
	// IPv4 IP address
	// For IPv6 address, use AF_INET6
	ifr.ifr_addr.sa_family = family;
	strncpy(ifr.ifr_name, interface, IFNAMSIZ - 1);
	ioctl(fd, SIOCGIFADDR, &ifr);
	close(fd);

	if (type == IPv4) {
		strncpy(intf_addr,
				inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr),
				IPV4_ADDR_LEN);
	} else {
		inet_ntop(family,
				(void *) &((struct sockaddr_in6 *) &ifr.ifr_addr)->sin6_addr,
				intf_addr, IPV6_ADDR_LEN);
	}

	return intf_addr;
}

/*
 * sstack_sfsd_pool_init - Initialize sfsd pool
 *
 * This function is called before sfsds are added to sfs by CLI.
 * This function initializes the pool list.
 *
 * Returns pointer to the pool list if successful. Otherwise returns NULL.
 */
inline sstack_sfsd_pool_t *
sstack_sfsd_pool_init(void)
{
	sstack_sfsd_pool_t *pool = NULL;
	sstack_sfsd_pool_t *temp = NULL;
	int i = 0;

	// Allocate memory for pools
	pool = (sstack_sfsd_pool_t *) calloc(sizeof(sstack_sfsd_pool_t),
					MAX_SFSD_POOLS);
	if (NULL == pool) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to allocate memory for "
						"sfsd pools.\n", __FUNCTION__);

		return NULL;
	}

	// Initialize individual members
	temp = pool;
	for (i = 0; i < MAX_SFSD_POOLS; i++) {
		// Following 5 lines are to avoid compilation error
		// due to weird C99 issue. These are repeated elsewhere
		uint32_t low = *(uint32_t *) &weights[i][0];
		uint32_t high = *(uint32_t *) &weights[i][1];

		temp->weight_range.low = low;
		temp->weight_range.high = high;
		INIT_LIST_HEAD((bds_list_head_t) &temp->list);
		pthread_spin_init(&temp->lock, PTHREAD_PROCESS_PRIVATE);
		temp ++;
	}

	return pool;
}

/*
 * sstack_sfsd_pool_destroy - Destroy sfsd pool
 *
 * pools - pointer to global sfsd pool structure
 *
 * Called during exit to clean up the data strctures.
 */

inline int
sstack_sfsd_pool_destroy(sstack_sfsd_pool_t *pools)
{
	int i = 0;
	sstack_sfsd_pool_t *temp = NULL;

	// Parameter validation
	if (NULL == pools) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parameter specified \n",
						__FUNCTION__);
		errno = EINVAL;
		return -1;
	}

	temp = pools;
	for (i = 0; i < MAX_SFSD_POOLS; i++) {
		temp->weight_range.low = 0;
		temp->weight_range.high = 0;
		// TODO
		// Free all the entries in the list befoe reinitializing the list
		INIT_LIST_HEAD((bds_list_head_t) &temp->list);
		pthread_spin_destroy(&temp->lock);
		temp ++;
	}

	free(pools);

	return 0;
}

/*
 * sstack_sfsd_add - Add sfsd to appropriate sfsd_pool
 *
 * weight - relative weight of the sfsd
 * pools - Global sfsd pool
 * sfsd - sfsd to be added to the pool
 *
 * Returns 0 on success and -1 on failure.
 */


inline int
sstack_sfsd_add(uint32_t weight, sstack_sfsd_pool_t *pools,
				sfsd_t *sfsd)
{
	sstack_sfsd_pool_t *temp = NULL;
	int i = 0;
	int index = -1;


	// Parameter validation
	if (weight < MAXIMUM_WEIGHT || weight > MINIMUM_WEIGHT ||
				NULL == pools || NULL == sfsd) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parametes specified \n",
						__FUNCTION__);

		errno = EINVAL;
		return -1;
	}

	// Get index of sfsd pool that covers the specified storeg weight
	for (i = 0; i < MAX_SFSD_POOLS; i++) {
		uint32_t low = *(uint32_t *) &weights[i][0];
		uint32_t high = *(uint32_t *) &weights[i][1];
		if (weight >= low && weight <= high)
			break;
	}
	index = i;
	temp = pools;

	for (i = 0; i < index; i++)
		temp ++;

	// Now temp points to the right pool
	pthread_spin_lock(&temp->lock);
	bds_list_add_tail((bds_list_head_t) &sfsd->list,
								(bds_list_head_t) &temp->list);
	pthread_spin_unlock(&temp->lock);

	return 0;
}


/*
 * sstack_sfsd_remove - Remove sfsd from sfsd pool
 *
 * wight - relative weight of sfsd
 * pools - Global sfsd pool
 * sfsd - sfsd to be removed
 *
 * This function should be called when the node running sfsd is decommissioned.
 *
 * Returns 0 on success and -1 on failure.
 */

inline int
sstack_sfsd_remove(uint32_t weight, sstack_sfsd_pool_t *pools,
					sfsd_t *sfsd)
{
	bds_list_head_t head = NULL;
	sstack_sfsd_pool_t *temp = NULL;
	int found = -1;
	int index = -1;
	int i = 0;
	sfsd_t *s = NULL;

	// Parameter validation
	if (weight < MAXIMUM_WEIGHT || weight > MINIMUM_WEIGHT ||
				NULL == pools || NULL == sfsd) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid parametes specified \n",
						__FUNCTION__);

		errno = EINVAL;
		return -1;
	}

	// Get index of sfsd pool that covers the specified storage weight
	for (i = 0; i < MAX_SFSD_POOLS; i++) {
		uint32_t low = *(uint32_t *) &weights[i][0];
		uint32_t high = *(uint32_t *) &weights[i][1];

		if (weight >= low && weight <= high)
			break;
	}
	index = i;
	temp = pools;

	for (i = 0; i < index; i++)
		temp ++;

	pthread_spin_lock(&temp->lock);
	if (list_empty((bds_list_head_t) &temp->list)) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Empty list at weight range %d %d. \n",
						__FUNCTION__, temp->weight_range.low,
						temp->weight_range.high);
		pthread_spin_unlock(&temp->lock);

		return -1;
	}

	pthread_spin_unlock(&temp->lock);
	head =(bds_list_head_t) &temp->list;
	pthread_spin_lock(&temp->lock);
	list_for_each_entry(s, head, list) {
		if (s == sfsd) {
			bds_list_del((bds_list_head_t) &s->list);
			found = 1;
			break;
		}
	}
	pthread_spin_unlock(&temp->lock);

	if (found != 1) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: sfsd 0x%llx not found in sfsd pool. "
						"Posible list corruption detected\n", __FUNCTION__,
						sfsd);
		return -1;
	}

	return 0;
}

