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
#include <policy.h>
#include <sfscli.h>

struct sfscli_cli_cmd *parse_fill_policy_input(int32_t argc, char *argv[])
{

	int32_t c;
	char *endptr = NULL;
	struct policy_input *pi;
	struct attribute *attr;
	struct sfscli_cli_cmd *p_cli = NULL;
	p_cli = malloc(sizeof(*p_cli));
	if (!p_cli)
		return NULL;

	memset(p_cli, 0, sizeof(*p_cli));
	/* Its a policy cmd */
	p_cli->cmd = SFSCLI_POLICY_CMD;
	pi = &p_cli->input.pi;
	attr = &pi->pi_attr;

	while(1) {
		int option_index = -1;
		/* The switch logic below uses this table indexes
		 * Be careful while changing this
		 */
		static struct option policy_longopts[] = {
			{"uid", required_argument, 0, 0}, /* index 0 */
			{"gid", required_argument, 0,0}, /* index 1 */
			{"fname", required_argument, 0, 0}, /* index 2 */
			{"ftype", required_argument, 0, 0}, /* index 3 */
			{"quota", required_argument, 0, 0}, /* index 4 */
			{"numrep", required_argument, 0, 0}, /* index 5 */
			{"dr", required_argument, 0, 0}, /* index 6 */
			{"qos", required_argument, 0, 0}, /* index 7 */
			{"hidden", required_argument, 0, 0}, /* index 8 */
			{"plugin", required_argument, 0, 0}, /* index 9 */
			{"add", no_argument, 0 ,'a'}, /* index 10 */
			{"delete", no_argument, 0, 'd'}, /* index 11 */
			{"list", no_argument, 0, 'l'}, /* index 12 */
			{0, 0, 0, 0}
		};

		c = getopt_long(argc, argv, "ald:",
						policy_longopts, &option_index);
		if (c == -1)
			break;

		switch(c) {
		case 'a':
			p_cli->input.policy_cmd = POLICY_ADD_CMD;
			break;
		case 'd':
			p_cli->input.policy_cmd = POLICY_DEL_CMD;
			break;
		case 'l':
			p_cli->input.policy_cmd = POLICY_SHOW_CMD;
			break;
		case '?':
			goto error;
		}

		/* The check is based on the indexes in the policy_longopts table
		 * Any change in the table **SHOULD** reflect here
		 */

		switch(option_index) {
		case 0:
			strtol_check_error_jump(pi->pi_uid, endptr, error, 10,
									optarg, option_index, policy_longopts);
			printf ("uid: %d\n", pi->pi_uid);
			break;
		case 1:
			strtol_check_error_jump(pi->pi_gid, endptr, error, 10, optarg,
									option_index, policy_longopts);
			printf ("gid: %d\n", pi->pi_gid);
			break;
		case 2:
			strncpy(pi->pi_fname, optarg, PATH_MAX);
			printf ("fname: %s\n", pi->pi_fname);
			break;
		case 3:
			strncpy(pi->pi_ftype, optarg, TYPE_LEN);
			printf ("ftype: %s\n", pi->pi_ftype);
			break;
		case 4:
			strtol_check_error_jump(attr->a_quota, endptr, error, 10,
									optarg, option_index, policy_longopts);
			printf ("quota: %d\n", attr->a_quota);
			break;
		case 5:
			strtol_check_error_jump(attr->a_numreplicas, endptr, error, 10,
									optarg,option_index, policy_longopts);
			printf ("num rep: %d\n", attr->a_numreplicas);
			break;
		case 6:
			strtol_check_error_jump(attr->a_enable_dr, endptr, error, 10,
									optarg, option_index, policy_longopts);
			printf ("is dr: %d\n", attr->a_enable_dr);
			break;
		case 7:
			printf("Qos: %s\n", optarg);
			if (!strncasecmp(optarg, "high", 5))
				attr->a_qoslevel = QOS_HIGH;
			else if (!strncasecmp(optarg, "medium", 6))
				attr->a_qoslevel = QOS_MEDIUM;
			else if (!strncasecmp(optarg, "low", 4))
				attr->a_qoslevel = QOS_LOW;
			else {
				fprintf (stderr, "Invalid qos level\n");
				goto error;
			}
			break;
		case 8:
			strtol_check_error_jump(attr->a_ishidden, endptr, error, 10,
									optarg, option_index, policy_longopts);
			printf ("is hidden: %d\n", attr->a_ishidden);
			break;
		case 9:
			strncpy(pi->pi_policy_tag[0], optarg, POLICY_TAG_LEN);
			printf ("plugin: %s\n", pi->pi_policy_tag[0]);
			break;
		}
	}

	/* No command to do for us!! */
	if (p_cli->input.policy_cmd == 0) {
		fprintf (stderr,
				 "Please specify atleast one of --add --delete --list\n");
		goto error;
	}

	return p_cli;
error:
	free(p_cli);
	return NULL;
}
