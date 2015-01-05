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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdint.h>
#include <sstack_types.h>
#include <sfscli.h>


struct sfscli_cli_cmd *
parse_fill_sfsd_input(int32_t argc, char *argv[])
{
	int32_t c;
	char *endptr = NULL;
	struct sfsd_input *si;
	struct sfscli_cli_cmd *p_cli = NULL;
	p_cli = malloc(sizeof(*p_cli));
	if (!p_cli)
		return NULL;

	memset(p_cli, 0, sizeof(*p_cli));
	/* Its storage cmd */
	p_cli->cmd = SFSCLI_SFSD_CMD;
	si = &p_cli->input.sdi;

	while(1) {
		int option_index = -1;
		/* Switch cases below use the option indexes.
		 * Please be careful when adding/deleting
		 * entries from it
		 */
		static struct option sfsd_longopts[] = {
			{"add", no_argument, 0, 'a'},
			{"decommision", no_argument, 0, 'd'},
			{"list", no_argument, 0, 'l'},
			{"address", required_argument, 0, 'A'},
			{"proto", required_argument, 0, 'p'},
			{0, 0, 0, 0}
		};

		c = getopt_long(argc, argv, "adlA:p:",
						 sfsd_longopts, &option_index);

		if (c == -1)
			break;

		switch(c) {
		case 'a':
			p_cli->input.sfsd_cmd = SFSD_ADD_CMD;
			printf ("sfsd add command\n");
			break;
		case 'd':
			p_cli->input.sfsd_cmd = SFSD_DECOMMISION_CMD;
			printf ("sfsd decommision command\n");
			break;
		case 'l':
			p_cli->input.sfsd_cmd = SFSD_SHOW_CMD;
			printf ("sfsd show command\n");
			break;
		case 'A':
			/* Support just IPv4 for now */
			si->address.protocol = IPV4;
			strncpy(si->address.ipv4_address, optarg, IPV4_ADDR_MAX);
			printf ("Address of sfsd node: %s\n", optarg);
			break;
		case 'p':
			strtol_check_error_jump(si->port, endptr, error, 10,
									optarg, option_index, sfsd_longopts);
			printf ("Port: %d\n", si->port);
			break;
		case '?':
			printf ("Received invalid character\n");
			goto error;
		}
	}

	if (p_cli->input.sfsd_cmd == 0) {
		fprintf (stderr,
				 "Please specify either one of --add --decommision"
				 "--list");
		goto error;
	}
	return p_cli;

error:
	free(p_cli);
	return NULL;
}
