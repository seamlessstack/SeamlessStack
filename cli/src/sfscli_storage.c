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

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <stdint.h>
#include <sstack_types.h>
#include <sfscli.h>


struct sfscli_cli_cmd *
parse_fill_storage_input(int32_t argc, char *argv[])
{
	int32_t c;
	char *endptr = NULL;
	struct storage_input *si;
	struct sfscli_cli_cmd *p_cli = NULL;
	p_cli = malloc(sizeof(*p_cli));
	if (!p_cli)
		return NULL;

	memset(p_cli, 0, sizeof(*p_cli));
	/* Its storage cmd */
	p_cli->cmd = SFSCLI_STORAGE_CMD;
	si = &p_cli->input.sti;

	while(1) {
		int option_index = -1;
		/* Switch cases below use the option indexes.
		 * Please be careful when adding/deleting
		 * entries from it
		 */
		static struct option storage_longopts[] = {
			{"add", no_argument, 0, 'a'}, /* index 0 */
			{"delete", no_argument, 0, 'd'}, /* index 1 */
			{"list", no_argument, 0, 'l'}, /* index 2 */
			{"update", no_argument, 0, 'u'}, /* index 3 */
			{"address", required_argument, 0, 'A'}, /* index 4 */
			{"remote-path", required_argument, 0, 'R'}, /* index 5 */
			{"proto", required_argument, 0, 'P'}, /* index 6 */
			{"type", required_argument, 0, 't'}, /*index 7 */
			{"size", required_argument, 0, 's'}, /* index 8 */
			{0, 0, 0, 0}
		};

		c = getopt_long(argc, argv, "adluA:R:P:t:s:",
						storage_longopts, &option_index);

		if (c == -1)
			break;

		switch(c) {
		case 'a':
			p_cli->input.storage_cmd = STORAGE_ADD_CMD;
			printf ("Storage add command\n");
			break;
		case 'd':
			p_cli->input.storage_cmd = STORAGE_DEL_CMD;
			printf ("Storage delete command\n");
			break;
		case 'l':
			p_cli->input.storage_cmd = STORAGE_SHOW_CMD;
			printf ("Storage list command\n");
			break;
		case 'u':
			p_cli->input.storage_cmd = STORAGE_UPDATE_CMD;
			printf ("Storage update command\n");
			break;
		case 'A':
			/* Support just IPv4 for now */
			si->address.protocol = IPV4;
			strncpy(si->address.ipv4_address, optarg, IPV4_ADDR_MAX);
			printf ("Address of storage node: %s\n", optarg);
			break;
		case 'R':
			/* The full path of the export */
			strncpy(si->rpath, optarg, PATH_MAX);
			printf ("Remote path: %s\n", optarg);
			break;
		case 'P':
			/* Access protocol */
			if (!strncasecmp(optarg, "NFS", 3))
				si->access_protocol = NFS;
			else if (!strncasecmp(optarg, "CIFS", 4))
				si->access_protocol = CIFS;
			else if (!strncasecmp(optarg, "ISCSI", 4))
				si->access_protocol = ISCSI;
			else if (!strncasecmp(optarg, "NATIVE", 6))
				si->access_protocol = NATIVE;
			else {
				fprintf (stderr, "Invalid storage protocol\n");
				goto error;
			}
			printf ("Access protocol: %s %d\n", optarg, si->access_protocol);
			break;
		case 't':
			/* Storage type */
			if (!strncasecmp(optarg, "HDD", 3))
				si->type = SSTACK_STORAGE_HDD;
			else if (!strncasecmp(optarg, "SSD", 3))
				si->type = SSTACK_STORAGE_SSD;
			else {
				fprintf (stderr,
						 "Invalid storage type, available types HDD/SSD\n");
				goto error;
			}
			printf ("Type of storage: %d\n", si->type);
			break;
		case 's':
			strtol_check_error_jump(si->size, endptr, error, 10,
									optarg, option_index, storage_longopts);
			printf ("size in Gigabytes: %ld\n", si->size);
			break;
		case '?':
			printf ("Received invalid character\n");
			goto error;
		}
	}

	if (p_cli->input.storage_cmd == 0) {
		fprintf (stderr,
				 "Please specify either one of --add"
				 "--delete --update or --list");
		goto error;
	}

	return p_cli;
error:
	free(p_cli);
	return NULL;
}
