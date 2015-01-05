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
