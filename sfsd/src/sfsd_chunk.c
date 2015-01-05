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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <inttypes.h>
#include <sstack_sfsd.h>
#include <sstack_chunk.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sstack_types.h>
#include <sys/mount.h>

#define MAX_COMMAND ((PATH_MAX) + (MAX_MOUNT_POINT_LEN) + 64)

static int32_t sfsd_chunk_schedule_rr(sfs_chunk_domain_t *chunk_domain);

/*
 * sfsd_chunk_domain_init - Initialize the chunk domain datastructure
 *
 * sfsd - sfsd to which this chunk domain belongs. This should be non-NULL
 * ctx  - Log context. Can be NULL if logging is not needed
 *
 * Returns NULL on failure and non-NULL address on success.
 */

sfs_chunk_domain_t *
sfsd_chunk_domain_init(sfsd_t *sfsd, log_ctx_t *ctx)
{
	sfs_chunk_domain_t * chunk = NULL;

	if (NULL == sfsd)
		return NULL;

	sfs_log(ctx, SFS_INFO, "%s: Creating chunk domain for sfsd 0x%llx\n",
			__FUNCTION__, (void*) sfsd);

	chunk = malloc(sizeof(sfs_chunk_domain_t));
	if (NULL == chunk) {
		sfs_log(ctx, SFS_ERR, "%s: Unable to allocate memory for chunk domain"
			" datastructure for sfsd 0x%llx. Out of memory\n",
			__FUNCTION__, (void *) sfsd);
		return NULL;
	}
	// Initialize the fields
	chunk->sfsd = sfsd;
	chunk->state = REACHABLE;	// Let's be positive :-)
	chunk->num_chunks = 0;
	chunk->size_in_blks = 0; // No chunks added yet
	chunk->ctx = ctx;
	chunk->storage = NULL;
	/* The chunk scheduler; For now it is simple round robin */
	chunk->schedule = sfsd_chunk_schedule_rr;

	sfs_log(ctx, SFS_INFO, "%s: Chunk domain for sfsd 0x%llx is successfully "
		"created. Chunk domain address = 0x%llx \n",
			__FUNCTION__, (void*) sfsd, (void*) chunk);
	return chunk;
}

/*
 * sfsd_chunk_domain_destroy - Destroy chunk domain
 *
 * chunk - Should be a non-NULL address
 *
 * Cleans up the chunk domain data structure. It is caller's responsibility
 * to set NULL to chunk domain field of the sfsd
 */

void
sfsd_chunk_domain_destroy(sfs_chunk_domain_t *chunk)
{
	if (NULL == chunk)
		return;

	sfs_log(chunk->ctx, SFS_INFO, "%s: Chunk domain for sfsd 0x%llx is"
		" destroyed \n", __FUNCTION__, (void *) chunk->sfsd);
	if (chunk->storage)
		free(chunk->storage);

	free(chunk);
}

/*
 * sfsd_add_chunk - Add a new chunk to chunk domain
 *
 * chunk - chunk domain address. Should be non-NULL
 * storage - chunk to be added to the chunk domain. Should be non-NULL
 *
 * Tries to mount the storage (if IPv4/IPv6) and returns mounted path as
 * return value. Returns NULL on failure.
 * chunk->storage gets updated with all the existing chunks plus new chunk.
 * So expect chunk->num_chunks to be incremented upon success.
 * storage->nblocks is assumed to be non-zero.
 *
 * NOTE:
 * Caller needs to free the return path.
 *
 */

char *
sfsd_add_chunk(sfs_chunk_domain_t *chunk, sfsd_storage_t *storage)
{
	//char command[MAX_COMMAND] = { '\0' };
	int ret = -1;
	char *path = NULL;

	if (NULL == chunk || NULL == storage)
		return NULL;


	// Try to mount the storage on a local path
	// Handling only TCP/IP for now
	if (storage->protocol == NFS) {
		char *updated_storage = NULL;
		char template[] = "/tmp/dirXXXXXX";
		char *path1 = NULL;
		char source[PATH_MAX + 20];
		char arguments[256];

		path = mkdtemp(template);
		if (NULL == path) {
			sfs_log(chunk->ctx, SFS_ERR, "%s: Failed to add chunk 0x%llx to "
				"chunk domain 0x%llx of sfsd 0x%llx. Creating mount point "
				"failed\n", __FUNCTION__, storage, chunk, chunk->sfsd);
			return NULL;
		}

		// ipv6_addr size takes care of both IPv4 and IPv6 addresses
		sfs_log(chunk->ctx, SFS_DEBUG, "%s: protocol %d ipaddr %s path %s\n",
			__FUNCTION__, storage->protocol, storage->address.ipv6_address,
			storage->path);

		/* Construct the arguments here */
		sprintf(source, "%s:%s", storage->address.ipv6_address, storage->path);
		sprintf(arguments, "addr=%s", storage->address.ipv6_address);
		sfs_log(chunk->ctx, SFS_DEBUG, "calling mount with %s %s %s\n",
				source, path, arguments);
		ret = mount(source, path, "nfs4", 0, arguments);
		if (ret == -1) {
			sfs_log(chunk->ctx, SFS_ERR, "%s: Failed to add chunk 0x%llx to "
				"chunk domain 0x%llx of sfsd 0x%llx. system failed with "
				"error %d\n", __FUNCTION__, storage, chunk, chunk->sfsd, errno);
			return NULL;
		}
		sfs_log(chunk->ctx, SFS_INFO, "%s: Mounting chunk path %s:%s to "
			"local path %s successded\n", __FUNCTION__,
			storage->address.ipv6_address,
			storage->path, path);

		// Update the chunk domain data structure

		// Allocate memory as number of chunks have increased
		// We could use realloc for chunk->storage. But the failure case
		// documentation in man page is bit amiguous. Not taking risk.
		updated_storage = calloc(1,
			sizeof(sfsd_storage_t) * (chunk->num_chunks +1 ));
		if (NULL == updated_storage) {
			sfs_log(chunk->ctx, SFS_ERR, "%s: Failed to add chunk 0x%llx to "
				"chunk domain 0x%llx of sfsd 0x%llx. Unable to allocate "
				"memory for updated storage chunk !!! \n",
				__FUNCTION__, storage, chunk, chunk->sfsd);
			free(path);
			return NULL;
		}
		// Copy existing chunks
		memcpy(updated_storage, (char *) chunk->storage,
			sizeof(sfsd_storage_t) * (chunk->num_chunks));
		// If there were preexisting chunks, free the memory as we have
		// Covered all of them.
		if (chunk->num_chunks)
			free(chunk->storage);
		chunk->storage = (sfsd_storage_t *)updated_storage;
		chunk->size_in_blks += storage->nblocks;
		strncpy((void *)storage->mount_point, (void *) path,
			MAX_MOUNT_POINT_LEN);
		// Copy new storage to chunk->num_chunks index.
		// This is bacuse we start at 0.
		memcpy(chunk->storage + chunk->num_chunks, storage,
			sizeof(sfsd_storage_t));
		chunk->num_chunks ++;
		path1 = strdup(path);

		return path1;
	} else {
		//TBD
		return NULL;
	}
}

/*
 * sfsd_remove_chunk - Remove an existing chunk from the chunk domain
 *
 * chunk - chunk domain address
 * storage - chunk to be removed
 *
 * Returns 0 on success and a negative number on failure. Logging is done
 * wherever possible.
 */

int
sfsd_remove_chunk(sfs_chunk_domain_t *chunk, sfsd_storage_t *storage)
{
	char *ptr = NULL;
	sfsd_storage_t *temp = NULL;
	int i = 0;
	int found = 0;
	char *new_storage = NULL;

	if (NULL == chunk || NULL == storage)
		return -1;

	if (chunk->num_chunks == 0) {
		// Chunk and chunk domain do not match
		sfs_log(chunk->ctx, SFS_ERR, "%s: Chunk 0x%llx does not belong to "
			"chunk domain 0x%llx \n", __FUNCTION__, storage, chunk);
		return -1;
	}

	// If there is only one chunk domain, free chunk->storage and update
	// chunk->size_in_blks
	if (chunk->num_chunks == 1) {
		chunk->num_chunks = 0;
		chunk->size_in_blks -= storage->nblocks;
		free(chunk->storage);

		return 0;
	}

	// Walk through the chunks in chunk domain and free up the chunk passed
	for(i = 0; i < chunk->num_chunks; i++) {
		temp = ((sfsd_storage_t *) chunk->storage) + i;
		// TODO
		// Only NFS handled
		if (storage->protocol == NFS) {
			if ((temp->path == storage->path) &&
				(temp->address.ipv6_address == storage->address.ipv6_address)) {
					found = 1;
					goto skip;
			}
		} else {
			// TODO
		}
	}
	if (!found) {
		sfs_log(chunk->ctx, SFS_ERR, "%s: Chunk 0x%llx does not belong to "
			"chunk domain 0x%llx \n", __FUNCTION__, storage, chunk);
		return -1;
	}
skip:
	// Adjust the chunks in chunk domain
	new_storage = calloc(1, (sizeof(sfsd_storage_t) * (chunk->num_chunks -1)));
	if (NULL == new_storage) {
		// Unable to allocate new memory
		// Try adjusting the existing chunk->storage itself
		// Side effect is one sizeof(sfsd_storage_t) is still attached to chunk
		// domain though not needed.
		if (i != (chunk->num_chunks -1)) {
			ptr = (char *) ((sfsd_storage_t *) chunk->storage + i);
			memmove(ptr, ptr + sizeof(sfsd_storage_t),
				((chunk->num_chunks -2) -i));
		} else {
			// Last chunk to be removed
			chunk->num_chunks -= 1;
			chunk->size_in_blks -= storage->nblocks;
		}
	} else {
		if (i == 0) {
			memcpy(new_storage, (char *) chunk->storage,
				sizeof(sfsd_storage_t) * (chunk->num_chunks -2));
			free(chunk->storage);
			chunk->storage = (sfsd_storage_t *) new_storage;
		} else {
			// Copy till i
			memcpy(new_storage, (void *) chunk->storage,
				sizeof(sfsd_storage_t) * (i+1));
			// Copy the rest if any
			if (i != (chunk->num_chunks -1)) {
				ptr = (char *) ((sfsd_storage_t *) chunk->storage + i);
				memmove(ptr, ptr + sizeof(sfsd_storage_t),
					((chunk->num_chunks -2) - i));
			}
			free(chunk->storage);
			chunk->storage = (sfsd_storage_t *) new_storage;
		}
	}

	// Upate chunk domain
	chunk->num_chunks -= 1;
	chunk->size_in_blks -= storage->nblocks;

	return 0;
}

/*
 * sfsd_update_chunk - Update blocks in a chunk
 *
 * chunk - chunk domain pointer
 * storage - chunk pointer
 *
 * Updates the size of the given chunk in chunk domain. It is assumed that
 * chunk pointer has updated nblocks.
 * This can be used to add some more capacity into existing chunk. This should
 * also be used after adding/removing/truncating an extent in the chunk.
 *
 * Returns 0 on success and a negative number on failure. Logging is done to
 * indicate failure.
 */


int
sfsd_update_chunk(sfs_chunk_domain_t *chunk, sfsd_storage_t *storage)
{
	int i = 0;
	sfsd_storage_t *temp = NULL;
	int found = 0;

	if (NULL == chunk || NULL == storage)
		return -1;

	if (chunk->num_chunks == 0) {
		// Chunk and chunk domain do not match
		sfs_log(chunk->ctx, SFS_ERR, "%s: Chunk 0x%llx does not belong to "
			"chunk domain 0x%llx \n", __FUNCTION__, storage, chunk);
		return -1;
	}

	for (i = 0; i < chunk->num_chunks; i++) {
		temp = chunk->storage + i;
		// TODO
		// Only NFS handled
		if (storage->protocol == NFS) {
			if ((temp->path == storage->path) &&
			    (!strncmp(temp->address.ipv6_address,
				      storage->address.ipv6_address,
				      IPV6_ADDR_MAX))) {
					found = 1;
					goto skip;
			}
		} else {
			// TODO
		}
	}
	if (found == 0) {
		sfs_log(chunk->ctx, SFS_ERR, "%s: Chunk 0x%llx does not belong to "
			"chunk domain 0x%llx \n", __FUNCTION__, storage, chunk);
		return -1;
	}
skip:
	sfs_log(chunk->ctx, SFS_INFO, "%s: Chunk size update for  chunk 0xllx "
		"of chunk domain 0x%llx succeeded. Old size 0x%lx new size 0x%lx \n",
		__FUNCTION__, storage, chunk, temp->nblocks, storage->nblocks);
	chunk->size_in_blks = ((chunk->size_in_blks + storage->nblocks) -
				temp->nblocks);
	temp->nblocks = storage->nblocks;

	return 0;
}

static inline char *
sfsd_disp_state(sfsd_state_t state)
{
	switch(state) {
		case INITIALIZING:
			return "INITIALIZING";
		case RUNNING:
			return "RUNNING";
		case REACHABLE:
			return "REACHABLE";
		case UNREACHABLE:
			return "UNREACHABLE";
		case DECOMMISSIONED:
			return "DECOMMISSIONED";
		default:
			return "UNKNOWN!!!";
	}
}

static inline char *
sfsd_disp_protocol(sfs_protocol_t protocol)
{
	switch(protocol) {
		case NFS:
			return "NFS";
		case CIFS:
			return "CIFS";
		case ISCSI:
			return "ISCSI";
		case NATIVE:
			return "NATIVE";
		default:
			return "UNKNOWN!!!";
	}
}

/*
 * sfsd_display_chunk_domain - Name explains all
 *
 * chunk - a non-NULL chunk domain pointer
 *
 * Logs errors/corruption if any. Otherwise logs the chunk domain information.
 */

void
sfsd_display_chunk_domain(sfs_chunk_domain_t *chunk)
{
	int i = 0;
	sfsd_storage_t *temp = NULL;

	if (NULL == chunk)
		return;

	sfs_log(chunk->ctx, SFS_INFO, "==============CHUNK DOMAIN DISPLAY =====\n");
	sfs_log(chunk->ctx, SFS_INFO, "sfsd = 0xllx \n", chunk->sfsd);
	sfs_log(chunk->ctx, SFS_INFO, "state = %s \n",
			sfsd_disp_state(chunk->state));
	sfs_log(chunk->ctx, SFS_INFO, "Number of chunks = %d \n",
			chunk->num_chunks);
	sfs_log(chunk->ctx, SFS_INFO, "Chunk domain size = %"PRId64" \n",
			chunk->size_in_blks);
	if (chunk->num_chunks == 0) {
		sfs_log(chunk->ctx, SFS_INFO, "No chunks attached \n");

		return;
	}
	sfs_log(chunk->ctx, SFS_INFO, "Chunks managed: \n");
	for (i = 0; i < chunk->num_chunks; i ++) {
		temp = chunk->storage + i;
		if (NULL == temp) {
			// chunk_storage and chunk->num_chunks do not match
			sfs_log(chunk->ctx, SFS_ERR, "%s: Chunk domain 0x%llx appears to"
				" be corrupted \n", __FUNCTION__, chunk);

			return;
		}
		sfs_log(chunk->ctx, SFS_INFO, "\nChunk # %d \n", i);
		sfs_log(chunk->ctx, SFS_INFO, "Chunk path = %s:%s\n",
			temp->address.ipv6_address, temp->path);
		sfs_log(chunk->ctx, SFS_INFO, "Chunk weight = %d\n", temp->weight);
		sfs_log(chunk->ctx, SFS_INFO, "Number of blocks in chunk = %"PRId64"\n",
			temp->nblocks);
		sfs_log(chunk->ctx, SFS_INFO, "Chunk protocol = %s\n",
			sfsd_disp_protocol(temp->protocol));
	}
}
/**
 * Round Robin chunk scheduler
 * returns the next usable chunk index
 * to be used
 **/

static int32_t sfsd_chunk_schedule_rr(sfs_chunk_domain_t *chunk_domain)
{
	static int32_t next_scheduled = 0;
	int32_t ret = -1;
	if (chunk_domain->num_chunks == 0) {
		/* No chunk added yet */
		sfs_log(chunk_domain->ctx, SFS_ERR,
			"%s(): line %d: No chunks added yet\n",
			__FUNCTION__, __LINE__);
		ret = -EINVAL;
	} else {
		sfs_log(chunk_domain->ctx, SFS_DEBUG,
			"%s(), line %d: Chunk scheduled %d\n",
			__FUNCTION__, __LINE__, next_scheduled);
		ret = (next_scheduled++) % chunk_domain->num_chunks;
	}

	return ret;
}

int32_t match_address(sstack_address_t *s1, sstack_address_t *s2)
{
	if (s1->protocol != s2->protocol)
		return FALSE;
	switch(s1->protocol)
	{
	case IPV4:
		if (!strncmp(s1->ipv4_address, s2->ipv4_address, IPV4_ADDR_MAX))
			return TRUE;
	case IPV6:
		if (!strncmp(s1->ipv6_address, s2->ipv6_address, IPV6_ADDR_MAX))
			return TRUE;
	default:
		return FALSE;
	};
	return FALSE;
}

/**
 * Get the mount point corresponding to the extent path passed
 * down as the parameter. Compare the "address" and the begining
 *  of the file path wth the nfs/<proto> export
 **/
char *get_mount_path(sfs_chunk_domain_t *chunk_domain ,
		     sstack_file_handle_t *file_handle, char **export_path)
{
	int32_t i;
	sfsd_storage_t *storage = NULL;
	if (chunk_domain == NULL || file_handle == NULL) {
		return NULL;
	}

	for (i = 0; i < chunk_domain->num_chunks; ++i) {
		storage = chunk_domain->storage + i;
		if (file_handle->proto != storage->protocol)
			continue;
		/* Match the IP addresses */
		/*if (match_address(&file_handle->address,
				  &storage->address) == FALSE)
			continue;*/
		if (strncmp(file_handle->address.ipv6_address,
					storage->address.ipv6_address, IPV6_ADDR_MAX) ||
				(file_handle->address.protocol != storage->address.protocol))
			continue;

		/* Protocol and addresses match. Let's try to match the nfs
		   mount path now */
		if (strstr(file_handle->name, storage->path)) {
			/* the match is found!!! */
			if (export_path)
				*export_path = storage->path;
			return storage->mount_point;
		}
	}
	return NULL;
}
