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

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sstack_types.h>
#include <sfs_internal.h>
#include <sstack_jobs.h>
#include <sstack_storage.h>
#include <sfscli.h>
#include <sfscli_clid.h>
#include <policy.h>
#include <sfs_storage_tree.h>
#include <sfs_jobid_tree.h>
#include <sfs_jobmap_tree.h>
#include <sstack_nfs.h>
#include <sstack_log.h>

#define MAX_RESPONSE_LEN	1000
char sfscli_response[MAX_RESPONSE_LEN];
static int32_t not_terminating = 1;
static int32_t connection_dropped = 1;
char *buf = NULL;
int	buf_len = 0;

static void handle_cli_requests(int32_t sockfd);

static sfs_st_t *
display_storage_devices(storage_tree_t *tree, sfs_st_t *node, void *arg)
{
	char dev_type[4];
	char proto[8];
	char *temp = NULL;
	static int index = 0;
	int	entry_len = 0;

	if (node == NULL) {
		sfs_log(sfs_ctx, SFS_ERR, "Critical error in tree\n");
		return NULL;
	}

	switch (node->type) {
		case SSTACK_STORAGE_HDD:
			strcpy(dev_type, "HDD");
			break;
		case SSTACK_STORAGE_SSD:
			strcpy(dev_type, "SSD");
			break;
		default:
			strcpy(dev_type, "ERR");
	}

	switch (node->access_protocol) {
		case NFS:
			strcpy(proto, "NFS");
			break;
		case CIFS:
			strcpy(proto, "CIFS");
			break;
		case ISCSI:
			strcpy(proto, "ISCSI");
			break;
		case NATIVE:
			strcpy(proto, "NATIVE");
			break;
		default:
			strcpy(proto, "ERR");
	}

	sprintf(sfscli_response, "Index: %d\t IP: %s   Remotepath: %s\t"
			"Proto: %s   Type: %s  Size: %ld\n",
			index++, node->address.ipv4_address,
			node->rpath, proto, dev_type, node->size);
	entry_len = strlen(sfscli_response);
	temp = realloc(buf, entry_len + buf_len);
	if (temp == NULL) {
		sfs_log(sfs_ctx, SFS_ERR, "%s: Line %d: Critical error\n",
				__FUNCTION__, __LINE__);
		return NULL;
	}
//	strncpy(temp, buf, buf_len);
	buf = temp;
	strncpy(buf + buf_len, sfscli_response, entry_len);
	buf_len += entry_len;

	return (NULL);
}

void *
cli_process_thread(void *arg)
{
	int32_t sockfd;
	int32_t optval;
	struct sockaddr_in servaddr;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd > 0) {
		optval = 1;
		setsockopt(sockfd,  SOL_SOCKET, SO_REUSEADDR,
				   (const void *)&optval, sizeof(int32_t));
		bzero(&servaddr, sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		servaddr.sin_addr.s_addr = INADDR_ANY;
		servaddr.sin_port = htons(atoi(SFSCLI_DEF_SFS_PORT));

		if (bind(sockfd, (struct sockaddr *)&servaddr,
					sizeof(servaddr)) == 0) {
			sfs_log(sfs_ctx, SFS_INFO, "Bind done, waiting for incoming "
					"connections %s\n", SFSCLI_DEF_SFS_PORT);
			listen(sockfd, 5);
			handle_cli_requests(sockfd);
		}
	}
	return NULL;
}

ssize_t get_policy_command_response(uint8_t *buffer, size_t buf_len,
									uint8_t **resp_buf)
{
	struct sfscli_cli_cmd *cmd;
	ssize_t	resp_len = 0;
	int		ret = -1;

	if (sfscli_deserialize_policy(buffer, buf_len, &cmd) != 0)
		return (0);

	switch (cmd->input.policy_cmd) {
		case POLICY_ADD_CMD:
			ret = submit_policy_entry(&(cmd->input.pi), db, POLICY_TYPE);
			if (ret < 0) {
				strncpy(sfscli_response, "CLI Error", MAX_RESPONSE_LEN);
				resp_len = strlen(sfscli_response);
				*resp_buf = malloc(resp_len);
				strncpy((char *) *resp_buf, sfscli_response, resp_len);
			} else {
				strncpy(sfscli_response, "Policy added successfully",
								MAX_RESPONSE_LEN);
				resp_len = strlen(sfscli_response);
				*resp_buf = malloc(resp_len);
				strncpy((char *) *resp_buf, sfscli_response, resp_len);
			}
			break;

		case POLICY_DEL_CMD:
			ret = delete_policy_entry(&(cmd->input.pi), db, POLICY_TYPE);
			if (ret < 0) {
				strncpy(sfscli_response, "Policy doesn't exist",
								MAX_RESPONSE_LEN);
				resp_len = strlen(sfscli_response);
				*resp_buf = malloc(resp_len);
				strncpy((char *) *resp_buf, sfscli_response, resp_len);
			} else {
				strncpy(sfscli_response, "Policy delete successfully",
								MAX_RESPONSE_LEN);
				resp_len = strlen(sfscli_response);
				*resp_buf = malloc(resp_len);
				strncpy((char *) *resp_buf, sfscli_response, resp_len);
			}
			break;

		case POLICY_SHOW_CMD:
			ret = show_policy_entries(resp_buf, &resp_len, db,
										POLICY_TYPE);
			if (ret < 0) {
				strncpy(sfscli_response, "CLI Error", MAX_RESPONSE_LEN);
				resp_len = strlen(sfscli_response);
				*resp_buf = malloc(resp_len);
				strncpy((char *) *resp_buf, sfscli_response, resp_len);
			}
			break;

		default:
			break;
	}

	return (resp_len);
}

// FIXME:
// Remove this
extern sfsd_t accept_sfsd;

ssize_t
get_storage_command_response(uint8_t *buffer, size_t buf_len,
	                                        uint8_t **resp_buf)
{
	struct sfscli_cli_cmd *cmd;
	ssize_t resp_len = 0;
	pthread_t thread_id;
	sstack_job_map_t *job_map = NULL;
	sfs_job_t *job = NULL;
	sstack_payload_t *payload = NULL;
	int		ret = 0;

	sfs_log(sfs_ctx, SFS_DEBUG, "%s %d\n", __FUNCTION__, __LINE__);
	if (sfscli_deserialize_storage(buffer, buf_len, &cmd) != 0) {
		sfs_log(sfs_ctx, SFS_DEBUG, "%s %d\n", __FUNCTION__, __LINE__);
		return (0);
	}

	sfs_log(sfs_ctx, SFS_DEBUG, "%s: command = %d\n", __FUNCTION__,
			cmd->input.storage_cmd);

	switch(cmd->input.storage_cmd) {
		case STORAGE_SHOW_CMD:
			sfs_log(sfs_ctx, SFS_DEBUG, "%s %d\n", __FUNCTION__, __LINE__);
			sfs_storage_tree_iter(display_storage_devices);
			strncpy((char *) *resp_buf, buf, buf_len);
			resp_len = buf_len;
			free(buf);
			buf_len = 0;
			break;

		case STORAGE_ADD_CMD:
		case STORAGE_DEL_CMD:
		case STORAGE_UPDATE_CMD:
		{
			sfs_log(sfs_ctx, SFS_DEBUG, "%s %d\n", __FUNCTION__, __LINE__);
			thread_id = pthread_self();
			job_map = create_job_map();
			if (NULL == job_map) {
		        sfs_log(sfs_ctx, SFS_ERR, "%s: Failed allocate memory for "
					                        "job map \n", __FUNCTION__);
				goto err;
			}

			job = sfs_job_init();
	        if (NULL == job) {
	            sfs_log(sfs_ctx, SFS_ERR,
						"%s: Fail to allocate job memory.\n", __FUNCTION__);
				goto err;
			}
			job->id = get_next_job_id();
			sfs_log(sfs_ctx, SFS_DEBUG, "%s %d job id 0x%x\n",
					__FUNCTION__, __LINE__, job->id);
	        job->job_type = SFSD_IO;
	        job->num_clients = 1;
			// FIXME:
			// Hardcoding sfsd pointer for now. This needs to go.

			job->sfsds[0] = &accept_sfsd;
			sfs_log(sfs_ctx, SFS_DEBUG, "%s: handle = %d \n",
					__FUNCTION__, job->sfsds[0]->handle);
//	        job->sfsds[0] = inode.i_primary_sfsd->sfsd;
	        job->job_status[0] = JOB_STARTED;

			job->priority = QOS_HIGH;
			payload = create_payload();
			payload->hdr.sequence = 0; // Reinitialized by transport
	        payload->hdr.payload_len = sizeof(sstack_payload_t);
	        payload->hdr.job_id = job->id;
	        payload->hdr.priority = job->priority;
	        payload->hdr.arg = (uint64_t) job;
			if (cmd->input.storage_cmd == STORAGE_ADD_CMD) {
				payload->command = SSTACK_ADD_STORAGE;
#if 0
				payload->command_struct.add_chunk_cmd.storage.address =
						cmd->input.sti.address;
				strcpy(payload->command_struct.add_chunk_cmd.storage.path,
						cmd->input.sti.rpath);
				payload->command_struct.add_chunk_cmd.storage.protocol =
						cmd->input.sti.access_protocol;
				payload->command_struct.add_chunk_cmd.storage.nblocks =
						cmd->input.sti.size;
				payload->command_struct.add_chunk_cmd.storage.weight =
						cmd->input.sti.type; //need to map type to weight TBD
#endif
				payload->storage.address =
						cmd->input.sti.address;
				strcpy(payload->storage.path,
						cmd->input.sti.rpath);
				payload->storage.protocol =
						cmd->input.sti.access_protocol;
				payload->storage.nblocks =
						cmd->input.sti.size;
				payload->storage.weight =
						cmd->input.sti.type; //need to map type to weight TBD
			} else if (cmd->input.storage_cmd == STORAGE_DEL_CMD) {
				payload->command = SSTACK_REMOVE_STORAGE;
#if 0
				payload->command_struct.delete_chunk_cmd.storage.address =
					                        cmd->input.sti.address;
				strcpy(payload->command_struct.delete_chunk_cmd.storage.path,
					                        cmd->input.sti.rpath);
#endif
				payload->storage.address = cmd->input.sti.address;
				strcpy(payload->storage.path, cmd->input.sti.rpath);

			} else if (cmd->input.storage_cmd == STORAGE_UPDATE_CMD) {
				payload->command = SSTACK_UPDATE_STORAGE;
#if 0
				payload->command_struct.update_chunk_cmd.storage.address =
					                        cmd->input.sti.address;
				strcpy(payload->command_struct.update_chunk_cmd.storage.path,
						                        cmd->input.sti.rpath);
				payload->command_struct.update_chunk_cmd.storage.nblocks =
					                        cmd->input.sti.size;
#endif
				payload->storage.address = cmd->input.sti.address;
				strcpy(payload->storage.path, cmd->input.sti.rpath);
				payload->storage.nblocks = cmd->input.sti.size;
			}

			job->payload_len = sizeof(sstack_payload_t);
	        job->payload = payload;
			job_map->num_jobs = 1;
			job_map->job_ids = malloc(sizeof(sstack_job_id_t));
	        if (NULL == job_map->job_ids) {
	            sfs_log(sfs_ctx, SFS_ERR, "%s: Failed to allocate memory for "
			                            "job_ids \n", __FUNCTION__);
				goto err;
			}
			job_map->job_ids[0] = job->id;
			ret = sfs_job2thread_map_insert(thread_id, job->id);
			if (ret == -1) {
	            sfs_log(sfs_ctx, SFS_ERR, "%s: Failed insert the job context "
			                            "into RB tree \n", __FUNCTION__);
				goto err;
			}
			ret = sfs_job_context_insert(thread_id, job_map);
		    if (ret == -1) {
		        sfs_log(sfs_ctx, SFS_ERR, "%s: Failed insert the job context "
				                        "into RB tree \n", __FUNCTION__);
		        sfs_job2thread_map_remove(thread_id);
				goto err;
			}
			ret = sfs_submit_job(job->priority, jobs, job);
	        if (ret == -1) {
				sfs_log(sfs_ctx, SFS_ERR, "%s: sfs_submit_job failed \n",
						__FUNCTION__);
	            sfs_job_context_remove(thread_id);
	            sfs_job2thread_map_remove(thread_id);
				goto err;
			}

			ret = sfs_wait_for_completion(job_map);
			sfs_log(sfs_ctx, SFS_DEBUG, "%s () - Job complete, error: %d\n",
					__FUNCTION__, job_map->err_no);
			if (job_map->err_no != SSTACK_SUCCESS)
				goto err;
			if (cmd->input.storage_cmd == STORAGE_ADD_CMD) {
				strncpy(sfscli_response, "Add storage successful",
									MAX_RESPONSE_LEN);
				sfs_storage_node_insert(cmd->input.sti.address,
						cmd->input.sti.rpath, cmd->input.sti.access_protocol,
						cmd->input.sti.type, cmd->input.sti.size);
			} else if (cmd->input.storage_cmd == STORAGE_DEL_CMD) {
				strncpy(sfscli_response, "Del storage successful",
			                          MAX_RESPONSE_LEN);
				sfs_storage_node_remove(cmd->input.sti.address,
										cmd->input.sti.rpath);
			} else if (cmd->input.storage_cmd == STORAGE_UPDATE_CMD) {
				strncpy(sfscli_response, "Update storage successful",
									MAX_RESPONSE_LEN);
				sfs_storage_node_update(cmd->input.sti.address,
                        cmd->input.sti.rpath, cmd->input.sti.size);
			}
			resp_len = strlen(sfscli_response);
			*resp_buf = malloc(resp_len);
			strncpy((char *) *resp_buf, sfscli_response, resp_len);

			sfs_job_context_remove(thread_id);
		    free(job_map->job_ids);
		    free_job_map(job_map);
			free(job);
			//free_payload(sfs_global_cache, payload);

			break;
		}

		default:
			sfs_log(sfs_ctx, SFS_ERR, "%s: Invalid command \n",
						__FUNCTION__);
			break;
	}
	sfs_log(sfs_ctx, SFS_DEBUG, "%s %d resp_len = %d\n",
			__FUNCTION__, __LINE__, resp_len);

	return (resp_len);
err:
	sfs_log(sfs_ctx, SFS_DEBUG, "CLI command error\n");
	if (job_map) {
		if (job_map->job_ids)
			free(job_map->job_ids);
		free_job_map(job_map);
	}
	if (job)
		free(job);
	if (payload)
		free_payload(sfs_global_cache, payload);
	strncpy(sfscli_response, "CLI Error", MAX_RESPONSE_LEN);
	resp_len = strlen(sfscli_response);
	*resp_buf = malloc(resp_len);
	strncpy((char *) *resp_buf, sfscli_response, resp_len);
	return (resp_len);
}

ssize_t get_license_command_response(uint8_t *buffer, size_t buf_len,
										uint8_t **resp_buf)
{
	struct sfscli_cli_cmd *cmd;
	ssize_t resp_len = 0;

	if (sfscli_deserialize_license(buffer, buf_len, &cmd) != 0)
		return (0);

	switch(cmd->input.license_cmd) {
		case LICENSE_ADD_CMD:
			break;

		case LICENSE_SHOW_CMD:
			break;

		case LICENSE_DEL_CMD:
			break;

		default:
			break;
	}

	return (resp_len);
}

ssize_t
get_sfsd_command_response(uint8_t *buffer, size_t buf_len, uint8_t **resp_buf)
{
	/* All the validation checks are done in get_command_response
	   No validation here
	*/
	struct sfscli_cli_cmd *cmd;

	if (sfscli_deserialize_sfsd(buffer, buf_len, &cmd) != 0)
		return 0;

	/* Now we have a valid cli_cmd structure in place, lets
	   process it */

	switch(cmd->input.sfsd_cmd) {
	case SFSD_ADD_CMD:
		break;
	default:
		sfs_log(sfs_ctx, SFS_ERR, "Not implemented\n");
	}

	// FIXME:
	return buf_len;

}

ssize_t
get_command_response(uint8_t *buffer, size_t buf_len, uint8_t **resp_buf)
{
	uint32_t magic = 0;
	sfscli_cmd_types_t cmd;
	uint8_t *p = buffer;
	size_t resp_len;

	sfs_log(sfs_ctx, SFS_DEBUG, "%s %d\n", __FUNCTION__, __LINE__);
	if (p == NULL) {
		sfs_log(sfs_ctx, SFS_ERR, "Invalid input buffer \n");
		return -ENOMEM;
	}

	/* Check for the magic */
	sfscli_deser_uint(magic, buffer, 4);

	if (magic != SFSCLI_MAGIC) {
		sfs_log(sfs_ctx, SFS_ERR, "Magic not found\n");
		return -EINVAL;
	}
	p+=4;
	sfs_log(sfs_ctx, SFS_DEBUG, "%s %d\n", __FUNCTION__, __LINE__);

	/* Peek into the command structure to
	 * see what command is it */
	sfscli_deser_nfield(cmd, p);
	sfs_log(sfs_ctx, SFS_DEBUG, "%s %d\n", __FUNCTION__, __LINE__);

	switch (cmd) {
	case SFSCLI_SFSD_CMD:
		resp_len = get_sfsd_command_response(buffer, buf_len, resp_buf);
		break;

	case SFSCLI_POLICY_CMD:
		resp_len = get_policy_command_response(buffer, buf_len, resp_buf);
		break;

	case SFSCLI_STORAGE_CMD:
		sfs_log(sfs_ctx, SFS_DEBUG, "%s %d\n", __FUNCTION__, __LINE__);
		resp_len = get_storage_command_response(buffer, buf_len, resp_buf);
		break;

	case SFSCLI_LICENSE_CMD:
		resp_len = get_license_command_response(buffer, buf_len, resp_buf);
		break;

	default:
		sfs_log(sfs_ctx, SFS_ERR, "Not implemented\n");
		resp_len = 0;
	}

	sfs_log(sfs_ctx, SFS_DEBUG, "%s %d\n", __FUNCTION__, __LINE__);
	return resp_len;
}

void
handle_cli_requests(int32_t sockfd)
{
	int32_t conn_sockfd = -1;
	struct sockaddr_in client_addr;
	socklen_t client_addr_len;
	uint8_t *cmd_buffer = NULL;
	uint8_t *resp_buffer = NULL;
	size_t rnbytes, resp_size, wnbytes;
	int32_t rc = -1;

	cmd_buffer = malloc(SFSCLI_MAX_BUFFER_SIZE);

	if (!cmd_buffer) {
		/* print a error message here */
		return;
	}

	while(not_terminating) {
		rnbytes = wnbytes = 0;
		memset(cmd_buffer, 0, SFSCLI_MAX_BUFFER_SIZE);
		sfs_log(sfs_ctx, SFS_ERR, "Waiting for an incoming connection\n");
		/* Accept connections and process */
		//if (connection_dropped == 1) {
			conn_sockfd = accept(sockfd,
								 (struct sockaddr *)&client_addr,
								 &client_addr_len);
			if (conn_sockfd < 0) {
				sfs_log(sfs_ctx, SFS_ERR, "coulnot accept the incoming "
						"connection\n");
				continue;
			}
			connection_dropped = 0;
			sfs_log(sfs_ctx, SFS_INFO, "Connection received from clid\n");
		//}

		rc = -1;
		select_read_to_buffer(conn_sockfd, rc, cmd_buffer,
							  SFSCLI_MAX_BUFFER_SIZE, rnbytes);
		if (rnbytes > 0 && rc > 0) {
			resp_size = get_command_response(cmd_buffer, rnbytes,
					&resp_buffer);
			if (resp_buffer && (resp_size > 0)) {
				wnbytes = write(conn_sockfd, resp_buffer, resp_size);
				if (wnbytes < resp_size)
					sfs_log(sfs_ctx, SFS_ERR, "Less no of response sent\n");
				else
					sfs_log(sfs_ctx, SFS_ERR, "Wrote %ld bytes to clid\n",
							wnbytes);
			}
		} else {
			sfs_log(sfs_ctx, SFS_ERR, "Error reading command\n");
		}

	}
}

#if 0
int
main(void)
{
	init_cli_thread(NULL);
}
#endif
