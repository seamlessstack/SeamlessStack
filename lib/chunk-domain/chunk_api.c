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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <inttypes.h>
#include <sstack_chunk.h>
#include <sys/stat.h>
#include <sys/types.h>
#define MAX_COMMAND ((PATH_MAX) + (MAX_MOUNT_POINT_LEN) + 64)

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
	char *path = NULL;
	char command[MAX_COMMAND] = { '\0' };
	int fd = -1;
	int ret = -1;

	if (NULL == chunk || NULL == storage)
		return NULL;

	path = calloc(1, MAX_MOUNT_POINT_LEN);
	if (NULL == path) {
		sfs_log(chunk->ctx, SFS_ERR, "%s: Failed to add chunk 0x%llx to "
			"chunk domain 0x%llx of sfsd 0x%llx. Out of memory !!! \n",
			__FUNCTION__, storage, chunk, chunk->sfsd);
		return NULL;
	}

	// Try to mount the storage on a local path
	// Handling only TCP/IP for now
	if (storage->protocol == NFS) {
		char *updated_storage = NULL;
		sfsd_storage_t *ptr = NULL;

		strcpy(path, "/tmp/tempXXXXXX");
		fd = mkstemp(path);
		if (fd == -1) {
			sfs_log(chunk->ctx, SFS_ERR, "%s: Failed to add chunk 0x%llx to "
				"chunk domain 0x%llx of sfsd 0x%llx. Creating mount point "
				"failed\n", __FUNCTION__, storage, chunk, chunk->sfsd);
			free(path);
			return NULL;
		}
		close(fd);
		if (mkdir(path, 0777) != 0) {
			sfs_log(chunk->ctx, SFS_ERR, "%s: Failed to add chunk 0x%llx to "
				"chunk domain 0x%llx of sfsd 0x%llx. mkdir failed with "
				"error %d\n", __FUNCTION__, storage, chunk, chunk->sfsd, errno);
			free(path);
			return NULL;
		}
		// Construct the NFS mount command
		// ipv6_addr size takes care of both IPv4 and IPv6 addresses

		snprintf((char *) command, MAX_COMMAND, "mount -t nfs %s:%s %s",
			storage->ipv6_addr, storage->path, path);
		ret = system(command);
		if (ret == -1) {
			sfs_log(chunk->ctx, SFS_ERR, "%s: Failed to add chunk 0x%llx to "
				"chunk domain 0x%llx of sfsd 0x%llx. system failed with "
				"error %d\n", __FUNCTION__, storage, chunk, chunk->sfsd, errno);
			free(path);
			return NULL;
		}
		sfs_log(chunk->ctx, SFS_INFO, "%s: Mounting chunk path %s:%s to "
			"local path %s successded\n", __FUNCTION__, storage->ipv6_addr,
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
		ptr = (sfsd_storage_t *) updated_storage;
		chunk->num_chunks ++;
		chunk->size_in_blks += storage->nblocks;
		ptr = chunk->storage + chunk->num_chunks;
		strncpy((void *)storage->mount_point, (void *) path,
			MAX_MOUNT_POINT_LEN);
		memcpy(chunk->storage + chunk->num_chunks, storage,
			sizeof(sfsd_storage_t));

		return path;
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
				(temp->ipv6_addr == storage->ipv6_addr)) {
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
				(temp->ipv6_addr == storage->ipv6_addr)) {
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
			temp->ipv6_addr, temp->path);
		sfs_log(chunk->ctx, SFS_INFO, "Chunk weight = %d\n", temp->weight);
		sfs_log(chunk->ctx, SFS_INFO, "Number of blocks in chunk = %"PRId64"\n",
			temp->nblocks);
		sfs_log(chunk->ctx, SFS_INFO, "Chunk protocol = %s\n",
			sfsd_disp_protocol(temp->protocol));
	}
}
