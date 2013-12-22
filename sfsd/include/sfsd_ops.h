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

#ifndef __SFSD_OPS_H_
#define __SFSD_OPS_H_

#include <bds_slab.h>

sstack_payload_t* sstack_getattr(
		sstack_payload_t *payload,
		log_ctx_t *ctx);

sstack_payload_t* sstack_setattr(
		sstack_payload_t *payload,
		log_ctx_t *ctx);

sstack_payload_t* sstack_lookup(
		sstack_payload_t *payload,
		log_ctx_t *ctx);

sstack_payload_t* sstack_access(
		sstack_payload_t *payload,
		log_ctx_t *ctx);

sstack_payload_t* sstack_readlink(
		sstack_payload_t *payload,
		log_ctx_t *ctx);

sstack_payload_t* sstack_read(
		sstack_payload_t *payload,
		sfsd_t *sfsd, log_ctx_t *ctx);

sstack_payload_t* sstack_write(
		sstack_payload_t *payload,
		sfsd_t *sfsd, log_ctx_t *ctx);

sstack_payload_t* sstack_create(
		sstack_payload_t *payload,
		sfsd_t *sfsd, log_ctx_t *ctx);

sstack_payload_t* sstack_mkdir(
		sstack_payload_t *payload,
		log_ctx_t *ctx);

sstack_payload_t* sstack_symlink(
		sstack_payload_t *payload,
		log_ctx_t *ctx);

sstack_payload_t* sstack_mknod(
		sstack_payload_t *payload,
		log_ctx_t *ctx);

sstack_payload_t* sstack_remove(
		sstack_payload_t *payload,
		log_ctx_t *ctx);

sstack_payload_t* sstack_rmdir(
		sstack_payload_t *payload,
		log_ctx_t *ctx);

sstack_payload_t* sstack_rename(
		sstack_payload_t *payload,
		log_ctx_t *ctx);

sstack_payload_t* sstack_link(
		sstack_payload_t *payload,
		log_ctx_t *ctx);

sstack_payload_t* sstack_readdir(
		sstack_payload_t *payload,
		log_ctx_t *ctx);

sstack_payload_t* sstack_readdirplus(
		sstack_payload_t *payload,
		log_ctx_t *ctx);

sstack_payload_t* sstack_fsstat(
		sstack_payload_t *payload,
		log_ctx_t *ctx);

sstack_payload_t* sstack_fsinfo(
		sstack_payload_t *payload,
		log_ctx_t *ctx);

sstack_payload_t* sstack_pathconf(
		sstack_payload_t *payload,
		log_ctx_t *ctx);

sstack_payload_t* sstack_commit(
		sstack_payload_t *payload,
		log_ctx_t *ctx);

sstack_payload_t* sstack_esure_code(
		sstack_payload_t *payload,
		sfsd_t *sfsd, log_ctx_t *ctx);
#endif /* __SFSD_OPS_H_ */
