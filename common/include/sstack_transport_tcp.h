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
