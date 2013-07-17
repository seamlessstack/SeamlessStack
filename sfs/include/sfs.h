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

#ifndef __SFS_H_
#define __SFS_H_

#include <sstack_log.h>
#define ROOT_SEP ":"
#define MINIMUM_WEIGHT 0
#define DEFAULT_WEIGHT 5
#define MAXIMUM_WEIGHT 100

typedef enum {
	KEY_BRANCHES,
	KEY_LOG_FILE_DIR,
	KEY_HELP,
	KEY_VERSION,
	KEY_LOG_LEVEL
} key_type_t;

typedef struct sfs_chunk_entry {
	char *chunk_path;
	int chunk_path_len;
	int chunk_fd;
	unsigned char rw;
	uint32_t weight; // weight associated with the branch
} sfs_chunk_entry_t;


#endif // __SFS_H_
