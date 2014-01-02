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

typedef enum {
	SFSCLI_POLICY_CMD = 1,
	SFSCLI_STORAGE_CMD = 2,
	SFSCLI_LICENSE_CMD = 3,
	SFSCLI_SFSD_CMD = 4,
	SFSCLI_MAX_CMD = 4,
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


struct sfscli_xxx_input {
	union {
		sfscli_policy_cmd_t policy_cmd;
		sfscli_sfsd_cmd_t sfsd_cmd;
		sfscli_storage_cmd_t storage_cmd;
		sfscli_license_cmd_t license_cmd;
	};
	union {
		struct policy_input pi;
		/*
		struct storage_input sti;
		struct sfsd_input sdi;
		struct license_input li;
		*/
	};
};

struct sfscli_cli_cmd {
	sfscli_cmd_types_t cmd;
	struct sfscli_xxx_input input;
};

struct sfscli_cli_cmd *parse_fill_policy_input(int32_t argc, char *argv[]);

#endif /* __SFSCLI_H_ */
