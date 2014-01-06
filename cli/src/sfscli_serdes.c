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
 * -----------------------------------------------------------------------------
 **/

int32_t sfscli_serialize_policy(struct sfscli_cli_cmd *cli_cmd,
								uint8_t **buffer)
{
	uint8_t *p = NULL, *q = NULL;
	struct policy_input *pi = &cli_cmd->input.pi;
	struct attribute *attr = &pi->pi_attr;

	p = malloc(sizeof(struct sfscli_cli_cmd) + 4 /* FOR MAGIC */
			   + (2 * (NUM_PI_FIELDS + NUM_ATTR_FIELDS
					   + NUM_XXX_INPUT_FIELDS + NUM_CLI_CMD_FIELDS)));

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


int32_t sfscli_deserialize_policy(uint8_t *buffer, size_t buf_len,
								  struct sfscli_cli_cmd **cli_cmd)
{
	struct sfscli_cli_cmd *cmd = NULL;
	struct policy_input *pi = NULL;
	struct attribute *attr = NULL;
	uint32_t magic = 0;
	uint8_t *p = buffer;

	/* Check for the magic */
	sfscli_deser_uint(magic, buffer, 4);

	if (magic != SFSCLI_MAGIC) {
		printf ("Magic not found\n");
		return -EINVAL;
	}
	p+=4;

	cmd = malloc(sizeof(*cmd));

	if (cmd == NULL) {
		printf ("Allocation failed:\n");
		return -ENOMEM;
	}

	pi = &cmd->input.pi;
	attr = &pi->pi_attr;

	/* cmd */
	sfscli_deser_nfield(cmd->cmd, p);
	/* pi->pi_uid */
	pi->pi_uid = 0;
	sfscli_deser_nfield(pi->pi_uid, p);
	/* pi->pi_gid */
	sfscli_deser_nfield(pi->pi_gid, p);
	/* pi->pi_policy_tag */
	sfscli_deser_sfield(pi->pi_policy_tag[0], p);
	/* pi->pi_fname */
	sfscli_deser_sfield(pi->pi_fname, p);
	/* pi->pi_ftype */
	sfscli_deser_sfield(pi->pi_ftype, p);
	/* pi->pi_num_policy */
	sfscli_deser_nfield(pi->pi_num_policy, p);
	/* attr->ver */
	sfscli_deser_nfield(attr->ver, p);
	/* attr->a_quota */
	sfscli_deser_nfield(attr->a_quota, p);
	/* attr->a_qoslevel */
	sfscli_deser_nfield(attr->a_qoslevel, p);
	/* attr->a_ishidden */
	sfscli_deser_nfield(attr->a_ishidden, p);
	/* attr->a_numreplicas */
	sfscli_deser_nfield(attr->a_numreplicas, p);
	/* attr->a_enable_dr */
	sfscli_deser_nfield(attr->a_enable_dr, p);

	*cli_cmd = cmd;
	return 0;
}


/**
 * Some comments about the serialization -
 * At the outset we have to serialize the structure sfscli_cli_cmd -
 * The structure looks like following :-
 * struct sfscli_cli_cmd
 *             |- cmd (enum)
 *             |-input (struct storage_input)
 *                 |-address (sstack_address_t)
 *                 |    |-protocol (enum)
 *                 |    |-ipv4_address/ipv6_address (enum)
 *                 |-rpath (char of len PATH_MAX)
 *                 |-access_protocol (enum)
 *                 |-type (enum)
 *                 |-size (uint64_t)
 * After serialization the serialized buffer looks like the following -
 *
 * -----------------------------------------------------------------------------
 * |MAGIC|sizeof(cmd)|cmd|sizeof(protocol)| protocol| ........................
 * -----------------------------------------------------------------------------
 **/
int32_t sfscli_serialize_storage(struct sfscli_cli_cmd *cli_cmd,
								 uint8_t **buffer)
{
	uint8_t *p = NULL, *q = NULL;
	struct storage_input *si = &cli_cmd->input.sti;

	p = malloc(sizeof(struct sfscli_cli_cmd) + 4 /* FOR MAGIC */
			   + (2 * (NUM_SI_FIELDS + NUM_SSTACK_ADDRESS_FIELDS
					   + NUM_XXX_INPUT_FIELDS + NUM_CLI_CMD_FIELDS)));

	if (p == NULL)
		return -ENOMEM;
	
	q = p;
	/* MAGIC (*/
	sfscli_ser_uint(SFSCLI_MAGIC, p, 4);
	p += 4;
	/* cmd */
	sfscli_ser_nfield(cli_cmd->cmd, p);
	/* si->address.protocol */
	sfscli_ser_nfield(si->address.protocol, p);
	/* si->address.ipv4_addr[] / si->address.ipv6_addr[] */
	if (si->address.protocol == IPV4)
		sfscli_ser_sfield(si->address.ipv4_address, p);
	else
		sfscli_ser_sfield(si->address.ipv6_address, p);

	/* si->rpath */
	sfscli_ser_sfield(si->rpath, p);
	/* si->access_protocol */
	sfscli_ser_nfield(si->access_protocol, p);
	/* si->type */
	sfscli_ser_nfield(si->type, p);
	/* si->size */
	sfscli_ser_nfield(si->size, p);
	*buffer = q;
	return ((int32_t) (p - q));
}


int32_t sfscli_deserialize_storage(uint8_t *buffer, size_t buf_len,
								   struct sfscli_cli_cmd **cli_cmd)
{
	struct sfscli_cli_cmd *cmd = NULL;
	struct storage_input *si = NULL;
	uint32_t magic = 0;
	uint8_t *p = buffer;

	/* Check for the magic */
	sfscli_deser_uint(magic, buffer, 4);

	if (magic != SFSCLI_MAGIC) {
		printf ("Magic not found\n");
		return -EINVAL;
	}
	p+=4;

	cmd = malloc(sizeof(*cmd));

	if (cmd == NULL) {
		printf ("Allocation failed:\n");
		return -ENOMEM;
	}

	si = &cmd->input.sti;

	/* cmd */
	sfscli_deser_nfield(cmd->cmd, p);
	/* si->address.protocol */
	sfscli_deser_nfield(si->address.protocol, p);
	/* si->address.ipv4_address[]/ si->address.ipv6_address[] */
	if (si->address.protocol == IPV4)
		sfscli_deser_sfield(si->address.ipv4_address, p);
	else
		sfscli_deser_sfield(si->address.ipv6_address, p);
	/* si->rpath */
	sfscli_deser_sfield(si->rpath, p);
	/* si->access_protocol */
	sfscli_deser_nfield(si->access_protocol, p);
	/* si->type */
	sfscli_deser_nfield(si->type, p);
	/* si->size */
	sfscli_deser_nfield(si->size, p);

	*cli_cmd = cmd;

	return 0;
}
		
int32_t sfscli_serialize_sfsd(struct sfscli_cli_cmd *cli_cmd,
							  uint8_t **buffer)
{
	uint8_t *p = NULL, *q = NULL;
	struct sfsd_input *si = &cli_cmd->input.sdi;

	p = malloc(sizeof(struct sfscli_cli_cmd) + 4 /* FOR MAGIC */
			   + (2 * (NUM_SDI_FIELDS + NUM_SSTACK_ADDRESS_FIELDS
					   + NUM_XXX_INPUT_FIELDS + NUM_CLI_CMD_FIELDS)));

	if (p == NULL)
		return -ENOMEM;
	
	q = p;
	/* MAGIC (*/
	sfscli_ser_uint(SFSCLI_MAGIC, p, 4);
	p += 4;
	/* cmd */
	sfscli_ser_nfield(cli_cmd->cmd, p);
	/* si->address.protocol */
	sfscli_ser_nfield(si->address.protocol, p);
	/* si->address.ipv4_addr[] / si->address.ipv6_addr[] */
	if (si->address.protocol == IPV4)
		sfscli_ser_sfield(si->address.ipv4_address, p);
	else
		sfscli_ser_sfield(si->address.ipv6_address, p);

	/* si->port */
	sfscli_ser_nfield(si->port, p);
	*buffer = q;
	return ((int32_t) (p - q));
}


int32_t sfscli_deserialize_sfsd(uint8_t *buffer, size_t buf_len,
								struct sfscli_cli_cmd **cli_cmd)
{
	struct sfscli_cli_cmd *cmd = NULL;
	struct sfsd_input *si = NULL;
	uint32_t magic = 0;
	uint8_t *p = buffer;

	/* Check for the magic */
	sfscli_deser_uint(magic, buffer, 4);

	if (magic != SFSCLI_MAGIC) {
		printf ("Magic not found\n");
		return -EINVAL;
	}
	p+=4;

	cmd = malloc(sizeof(*cmd));

	if (cmd == NULL) {
		printf ("Allocation failed:\n");
		return -ENOMEM;
	}

	si = &cmd->input.sdi;

	/* cmd */
	sfscli_deser_nfield(cmd->cmd, p);
	/* si->address.protocol */
	sfscli_deser_nfield(si->address.protocol, p);
	/* si->address.ipv4_address[]/ si->address.ipv6_address[] */
	if (si->address.protocol == IPV4)
		sfscli_deser_sfield(si->address.ipv4_address, p);
	else
		sfscli_deser_sfield(si->address.ipv6_address, p);
	/* si->port */
	sfscli_deser_nfield(si->port, p);

	*cli_cmd = cmd;

	printf ("command: %d\n", cmd->cmd);
    printf ("Address type: %d\n", si->address.protocol);
	printf ("Address string: %s\n", si->address.ipv4_address);
	printf ("Port: %d\n", si->port);

	return 0;
}

int32_t sfscli_serialize_license(struct sfscli_cli_cmd *cli_cmd,
								 uint8_t **buffer)
{
	uint8_t *p = NULL, *q = NULL;
	struct license_input *si = &cli_cmd->input.li;

	p = malloc(sizeof(struct sfscli_cli_cmd) + 4 /* FOR MAGIC */
			   + (2 * (NUM_LI_FIELDS + NUM_SSTACK_ADDRESS_FIELDS
					   + NUM_XXX_INPUT_FIELDS + NUM_CLI_CMD_FIELDS)));

	if (p == NULL)
		return -ENOMEM;
	
	q = p;
	/* MAGIC (*/
	sfscli_ser_uint(SFSCLI_MAGIC, p, 4);
	p += 4;
	/* cmd */
	sfscli_ser_nfield(cli_cmd->cmd, p);
	/* li->lisence_path */
	sfscli_ser_sfield(si->license_path, p);
	return ((int32_t) (p - q));
}


int32_t sfscli_deserialize_license(uint8_t *buffer, size_t buf_len,
								   struct sfscli_cli_cmd **cli_cmd)
{
	struct sfscli_cli_cmd *cmd = NULL;
	struct license_input *li = NULL;
	uint32_t magic = 0;
	uint8_t *p = buffer;

	/* Check for the magic */
	sfscli_deser_uint(magic, buffer, 4);

	if (magic != SFSCLI_MAGIC) {
		printf ("Magic not found\n");
		return -EINVAL;
	}
	p+=4;

	cmd = malloc(sizeof(*cmd));

	if (cmd == NULL) {
		printf ("Allocation failed:\n");
		return -ENOMEM;
	}

	li = &cmd->input.li;

	/* cmd */
	sfscli_deser_nfield(cmd->cmd, p);
	/* li->license_path */
	sfscli_deser_sfield(li->license_path, p);

	*cli_cmd = cmd;

	printf ("Command: %d\n", cmd->cmd);
	printf ("License path: %s\n", li->license_path);

	return 0;
}
