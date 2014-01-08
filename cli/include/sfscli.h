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

#ifndef __SFSCLI_H_
#define __SFSCLI_H_

#include <policy.h>
#include <sstack_types.h>

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
		FD_SET(readsockfd, &readsockfds);								\
		rc = select(readsockfd + 1, &readsockfds, NULL, NULL, NULL);	\
		if (rc == -1)													\
			break;														\
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
	for(int32_t i = 0; i < num_bytes; ++i) {							\
		int32_t shifter = (i<<3);										\
		val |= (buffer[i] << shifter);									\
	};																	\

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
