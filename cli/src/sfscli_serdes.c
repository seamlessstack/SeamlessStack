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

#include <stdint.h>
#include <policy.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sfscli.h>


/**
 * Some comments about the serialization -
 * At the outset we have to serialize the structure sfscli_cli_cmd -
 * The structure looks like following :-
 * struct sfscli_cli_cmd
 *             |- cmd (enum)
 *             |-input (struct policy_input)
 *                 |-pi_uid (uid_t)
 *                 |-pi_gid (gid_t)
 *                 |-pi_policy_tag (char of len POLICY_TAX_LEN)
 *                 |-pi_fname (char of PATH_MAX)
 *                 |-pi_ftype (char of TYPE_LEN)
 *                 |-pi_num_policy (size_t)
 *                 |-attr
 *                    |-ver (uint32_t)
 *                    |-a_quota (uint32_t)
 *                    |-a_qoslevel (uint8_t)
 *                    |-a_ishidden (uint8_t)
 *                    |-a_numreplicas (uint8_t)
 *                    |-a_enable_dr (uint8_t)
 * After serialization the serialized buffer looks like the following -
 *
 * -----------------------------------------------------------------------------
 * |MAGIC|sizeof(cmd)|cmd|sizeof(uid_t)|pi_uid|sizeof(gid_t)|pi_gid|............
 * ------------------------------------------------------------------------------
 **/
int32_t sfscli_serialize_policy(struct sfscli_cli_cmd *cli_cmd, uint8_t **buffer)
{
	uint8_t *p = NULL, *q = NULL;
	struct policy_input *pi = &cli_cmd->input.pi;
	struct attribute *attr = &pi->pi_attr;
	
	p = malloc(sizeof(struct sfscli_cli_cmd) + 4 /* FOR MAGIC */
			   + (2 * (NUM_PI_FIELDS + NUM_PI_FIELDS)));

	if (p == NULL)
		return -ENOMEM;

	q = p;
	/* MAGIC (*/
	sfscli_ser_uint(SFSCLI_MAGIC, p, 4);
	p += 4;
	/* cmd */
	sfscli_ser_nfield(cli_cmd->cmd, p);
	/* pi->pi_uid */
	sfscli_ser_nfield(pi->pi_uid, p);
	/* pi->pi_gid */
	sfscli_ser_nfield(pi->pi_gid, p);
	/* pi->pi_policy_tag */
	sfscli_ser_sfield(pi->pi_policy_tag[0], p);
	/* pi->pi_fname */
	sfscli_ser_sfield(pi->pi_fname, p);
	/* pi->pi_ftype */
	sfscli_ser_sfield(pi->pi_ftype, p);
	/* pi->pi_num_policy */
	sfscli_ser_nfield(pi->pi_num_policy, p);
	/* attr->ver */
	sfscli_ser_nfield(attr->ver, p);
	/* attr->a_quota */
	sfscli_ser_nfield(attr->a_quota, p);
	/* attr->a_qoslevel */
	sfscli_ser_nfield(attr->a_qoslevel, p);
	/* attr->a_ishidden */
	sfscli_ser_nfield(attr->a_ishidden, p);
	/* attr->a_numreplicas */
	sfscli_ser_nfield(attr->a_numreplicas, p);
	/* attr->a_enable_dr */
	sfscli_ser_nfield(attr->a_enable_dr, p);
	*buffer = q;
	return ((int32_t)(p - q));
	
}
