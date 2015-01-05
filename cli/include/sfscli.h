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

#ifndef __SFSCLI_H_
#define __SFSCLI_H_

#include <policy.h>
#include <sstack_types.h>
#include <time.h>

#define SFSCLI_MAGIC 0xdeadbeef
#define SFSCLI_MAGIC_LEN sizeof(SFSCLI_MAGIC);
#define SFSCLI_TLV_MAGIC 0xcf

#define strtol_check_error_jump(assignee, endptr, jump_label,			\
								base, string, option_index, longopts)	\
	do {																\
		endptr = NULL;													\
		assignee = strtol(string, &endptr, base);						\
		if (endptr != NULL && endptr[0] != '\0') {						\
			fprintf(stderr, "Invalid %s\n", longopts[option_index].name); \
			goto jump_label;											\
		}																\
	} while (0);


#define select_read_to_buffer(readsockfd, rc, buffer, buf_size,			\
							  read_bytes)								\
	for (;;) {															\
		fd_set readsockfds;												\
		struct timeval tv	;											\
		FD_SET(readsockfd, &readsockfds);								\
		tv.tv_sec = 1; tv.tv_usec = 0;									\
		rc = select(readsockfd + 1, &readsockfds, NULL, NULL, &tv);		\
		if (rc == -1) {													\
			sleep(1);													\
			break;														\
		}																\
		if (FD_ISSET(readsockfd, &readsockfds)) {						\
			rnbytes = read(readsockfd, buffer, buf_size);				\
			if (rnbytes > 0)											\
				break;													\
		}																\
	}
	
typedef enum {
	SFSCLI_POLICY_CMD = 1,
	SFSCLI_STORAGE_CMD = 2,
	SFSCLI_LICENSE_CMD = 3,
	SFSCLI_SFSD_CMD = 4,
	SFSCLI_CLID_TEARDOWN = 5,
	SFSCLI_MAX_CMD = 5,
} sfscli_cmd_types_t;

typedef enum {
	POLICY_ADD_CMD = 1,
	POLICY_DEL_CMD = 2,
	POLICY_LIST_CMD = 3,
	POLICY_SHOW_CMD = 4,
	POLICY_MAX_CMD = 4,
} sfscli_policy_cmd_t;

typedef enum {
	LICENSE_ADD_CMD = 1,
	LICENSE_SHOW_CMD = 2,
	LICENSE_DEL_CMD = 3,
	LICENSE_MAX_CMD = 3,
} sfscli_license_cmd_t;

typedef enum {
	SFSD_ADD_CMD = 1,
	SFSD_DECOMMISION_CMD = 2,
	SFSD_SHOW_CMD = 3,
	SFSD_MAX_CMD = 3,
} sfscli_sfsd_cmd_t;

typedef enum {
	STORAGE_ADD_CMD = 1,
	STORAGE_DEL_CMD = 2,
	STORAGE_UPDATE_CMD = 3,
	STORAGE_SHOW_CMD = 4,
	STORAGE_MAX_CMD = 4,
} sfscli_storage_cmd_t;


struct storage_input {
	sstack_address_t address;
	char rpath[PATH_MAX];
	sfs_protocol_t access_protocol;
	sstack_storage_type_t type;
	size_t size;

	/* Please change this when modifying this structure
	   This lists the number of elements in the structure
	*/
#define NUM_SI_FIELDS 5
};

struct sfsd_input {
	sstack_address_t address;
	uint16_t port;
	/* Please change the below define to the number of
	   fields in the structure when changing this structure
	*/
#define NUM_SDI_FIELDS 2
};

struct license_input {
	char license_path[PATH_MAX];
	/* Please change the below define to the number of
	   fields in the structure when changing this structure
	*/
#define NUM_LI_FIELDS 1
};

struct sfscli_xxx_input {
	union {
		sfscli_policy_cmd_t policy_cmd;
		sfscli_storage_cmd_t storage_cmd;
		sfscli_sfsd_cmd_t sfsd_cmd;
		sfscli_license_cmd_t license_cmd;
	};
	union {
		struct policy_input pi;
		struct storage_input sti;
		struct sfsd_input sdi;
		struct license_input li;
	};
	/* Please change this when modifying this structure
	   This lists the number of elements in the structure
	*/
#define NUM_XXX_INPUT_FIELDS 2
};

struct sfscli_cli_cmd {
	sfscli_cmd_types_t cmd;
	struct sfscli_xxx_input input;
	/* Please change this when modifying this structure
	   This lists the number of elements in the structure
	*/
#define NUM_CLI_CMD_FIELDS 2
};

/* ================== Serialization Macros =========================== */

#define sfscli_ser_uint(val, buffer, num_bytes)				\
	for(int i = 0; i < num_bytes; ++i) {					\
		int64_t shifter = (i << 3);							\
		buffer[i] = ((val & (0xFF << shifter)) >> shifter);	\
	}

#define sfscli_ser_nfield(field, buffer)								\
	do {																\
		uint16_t size = sizeof(field);									\
		sfscli_ser_uint(size, buffer, 2);								\
		buffer+=2;														\
		sfscli_ser_uint(field, buffer, size);							\
		buffer += size;													\
	} while(0)

#define sfscli_ser_sfield(field, buffer)								\
	do {																\
		uint16_t size = strlen(field);									\
		sfscli_ser_uint(size, buffer, 2);								\
		buffer+=2;														\
		memcpy(buffer, field, size);									\
		buffer += size;													\
	} while(0)

/* ================= Deserialization Macros ========================= */

#define sfscli_deser_uint(val, buffer, num_bytes)						\
	{																	\
		int32_t i;														\
		for(i = 0; i < num_bytes; ++i) {						\
			int32_t shifter = (i<<3);									\
			val |= (buffer[i] << shifter);								\
		};																\
	}																	\

#define sfscli_deser_nfield(field, buffer)								\
	do {																\
		uint16_t size = 0;												\
		sfscli_deser_uint(size, buffer, 2);								\
		buffer+=2;														\
		sfscli_deser_uint(field, buffer, size);							\
		buffer+=size;													\
	} while (0);

#define sfscli_deser_sfield(field, buffer)		\
	do {										\
		uint16_t size = 0;						\
		sfscli_deser_uint(size, buffer, 2);		\
		buffer+=2;								\
		memcpy(field, buffer, size);			\
		buffer += size;							\
	} while (0)


struct sfscli_cli_cmd *parse_fill_policy_input(int32_t argc, char *args[]);
struct sfscli_cli_cmd *parse_fill_storage_input(int32_t argc, char *args[]);
struct sfscli_cli_cmd *parse_fill_sfsd_input(int32_t argc, char *args[]);
struct sfscli_cli_cmd *parse_fill_license_input(int32_t argc, char *args[]);

int32_t sfscli_serialize_policy(struct sfscli_cli_cmd *cli_cmd,
								uint8_t **buffer);
int32_t sfscli_deserialize_policy(uint8_t *buffer, size_t buf_len,
								  struct sfscli_cli_cmd **cli_cmd);

int32_t sfscli_serialize_storage(struct sfscli_cli_cmd *cli_cmd,
								 uint8_t **buffer);
int32_t sfscli_deserialize_storage(uint8_t *buffer, size_t buf_len,
								   struct sfscli_cli_cmd **cli_cmd);

int32_t sfscli_serialize_sfsd(struct sfscli_cli_cmd *cli_cmd,
							  uint8_t **buffer);
int32_t sfscli_deserialize_sfsd(uint8_t *buffer, size_t buf_len,
								struct sfscli_cli_cmd **cli_cmd);

int32_t sfscli_serialize_license(struct sfscli_cli_cmd *cli_cmd,
								 uint8_t **buffer);
int32_t sfscli_deserialize_license(uint8_t *buffer, size_t buf_len,
								   struct sfscli_cli_cmd **cli_cmd);

#endif /* __SFSCLI_H_ */
