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
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdint.h>
#include <sstack_types.h>
#include <sfscli.h>


struct sfscli_cli_cmd *parse_fill_license_input(int32_t argc, char *argv[])
{
	int32_t c;
	struct license_input *li;
	struct sfscli_cli_cmd *p_cli = NULL;
	p_cli = malloc(sizeof(*p_cli));
	if (!p_cli)
		return NULL;

	memset(p_cli, 0, sizeof(*p_cli));
	/* Its storage cmd */
	p_cli->cmd = SFSCLI_LICENSE_CMD;
	li = &p_cli->input.li;

	while(1) {
		int option_index = -1;
		/* Switch cases below use the option indexes.
		 * Please be careful when adding/deleting
		 * entries from it
		 */
		static struct option license_longopts[] = {
			{"add", no_argument, 0, 'a'},
			{"list", no_argument, 0, 'l'},
			{"delete", no_argument, 0, 'd'},
			{"path", required_argument, 0, 'p'},
			{0, 0, 0, 0}
		};

		c = getopt_long(argc, argv, "aldp:",
						license_longopts, &option_index);

		if (c == -1)
			break;

		switch(c) {
		case 'a':
			p_cli->input.license_cmd = LICENSE_ADD_CMD;
			printf ("License add command\n");
			break;
		case 'd':
			p_cli->input.license_cmd = LICENSE_DEL_CMD;
			printf ("License delete cmd\n");
			break;
		case 'l':
			p_cli->input.license_cmd = LICENSE_SHOW_CMD;
			printf ("license show cmd\n");
		case 'p':
			strncpy(li->license_path, optarg, PATH_MAX);
			printf ("license path: %s\n", optarg);
			break;
		case '?':
			printf ("Invalid character received\n");
			goto error;
		}
	}

	if (p_cli->input.license_cmd == 0) {
		fprintf(stderr, "Please specify one of --add --delete --list");
		goto error;
	}

	return p_cli;

error:
	free(p_cli);
	return NULL;
}
