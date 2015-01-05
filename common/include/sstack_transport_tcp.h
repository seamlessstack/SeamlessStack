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

#ifndef _SSTACK_TRANSPORT_TCP_H__
#define _SSTACK_TRANSPORT_TCP_H__
#include <stdint.h>
#include <sstack_transport.h>
#include <sstack_types.h>
#include <sstack_log.h>

extern sstack_client_handle_t tcp_client_init(sstack_transport_t *);
extern int tcp_rx(sstack_client_handle_t , size_t , void * , log_ctx_t *);
extern int tcp_tx(sstack_client_handle_t , size_t , void * , log_ctx_t *);
extern int tcp_select(sstack_client_handle_t , uint32_t );
extern sstack_client_handle_t tcp_server_setup(sstack_transport_t *);
extern sstack_transport_t *get_tcp_transport(char *, log_ctx_t *);

extern inline int sstack_transport_register(sstack_transport_type_t ,
				sstack_transport_t *, sstack_transport_ops_t );
extern inline int sstack_transport_deregister(sstack_transport_type_t ,
				sstack_transport_t *);

#endif // _SSTACK_TRANSPORT_TCP_H__
