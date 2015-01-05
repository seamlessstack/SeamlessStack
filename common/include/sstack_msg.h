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

#ifndef _SSTACK_MSG_H_
#define _SSTACK_MSG_H_

#include <stdint.h>
#include <inttypes.h>
#include <sstack_log.h>
#include <openssl/sha.h>

#define MSG_VERSION 101 // Some arbitrary number

/*
 * This file defines inter process messaging infrastructure.
 * This is a generic infrastructure and depends on underlying transport
 * to handle actual message sending.
 */

typedef unsigned int sstack_msg_version_t;
typedef unsigned long long sstack_htbt_sequence_t;

typedef enum sstack_msg_type {
	HTBT = 1,  // Used for heartbeat
	SUICIDE = 2, // Used during graceful shutdown
	CACHE_FILL = 3, // Fill new cache line with data
	CACHE_FLUSH = 4, // Flush particular cache line to disk
	CACHE_INV_ONE = 5, // Invalidate a cache line
	CACHE_INV_ALL = 6, // Invalidate all cache lines
	UPD_SEQUENCE = 7, // Update sequence number
} sstack_msg_type_t;

typedef struct sstack_cache_fill {
	uint8_t hashkey[SHA256_DIGEST_LENGTH + 1]; // Hash key for filename + offset
	uint8_t data[MAX_EXTENT_SIZE];
} sstack_cache_fill_t;

typedef struct sstack_cache_flush {
	uint8_t hashkey[SHA256_DIGEST_LENGTH + 1];
} sstack_cache_flush_t;

typedef struct sstack_cache_inv_one {
	uint8_t hashkey[SHA256_DIGEST_LENGTH + 1];
} sstack_cache_inv_one_t;

// CACHE_INV_ALL does not require any payload.
// Just send the hdr for invalidating all cache lines
// This is a rare occurrence. Mostly needed for graceful shutdown.


typedef enum sstack_service_type {
	SFS = 1,
	SFSD = 2,
} sstack_service_type_t;


typedef struct sstack_msg_hdr {
	sstack_msg_version_t msg_ver; // Version of client
	uint32_t msg_crc; // CRC for the message including header
	sstack_msg_type_t msg_type; // Type of the message
	uint32_t payload_len; // Length of payload
} sstack_msg_hdr_t;

typedef struct sstack_htbt {
	sstack_htbt_sequence_t sequence; // Current sequence number
} sstack_htbt_t;

typedef struct sstack_suicide {
	sstack_service_type_t stype; // To indicate who is going down
} sstack_suicide_t;


// Message should be one of the sstack_msg_type_t
// Fill in the data accordingly.

typedef struct sstack_msg {
	sstack_msg_hdr_t hdr;
	uint8_t data[0]; // Data to be allocated based on msg type
} sstack_msg_t;
	
#endif // _SSTACK_MSG_H_
