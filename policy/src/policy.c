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
s * patents in process, and are protected by trade secret or copyright law.
 * Dissemination of this information or reproduction of this material
 * is strictly forbidden unless prior written permission is obtained
 * from SeamlessStack Incorporated.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <ctype.h>
#include <Judy.h>
#include <pthread.h>
#include <policy.h>
#include <sstack_db.h>
#include <sstack_log.h>


#define NUM_BUCKETS 16
#define TYPE_LEN 8

#define POLICY_FILE "/home/shubhro/policy/policy.conf"
/*=================== PRIVATE STRUCTURES AND FUNCTIONS ===================*/
struct policy_input
{
	uid_t	pi_uid;
	gid_t	pi_gid;
	char	pi_policy_tag[NUM_MAX_POLICY][POLICY_TAG_LEN];
	char	pi_ftype [TYPE_LEN];
	char	pi_fname[PATH_MAX];
	size_t	pi_num_policy;
	struct attribute pi_attr;
};

struct policy_db_entry
{
	char pdb_policy_tag[NUM_MAX_POLICY][POLICY_TAG_LEN];
	char pdb_policy_cksum[NUM_MAX_POLICY][SHA256_DIGEST_LENGTH];
	uint8_t pdb_mem_index;
	size_t pdb_num_policy;
	struct attribute pdb_attr;
};

#if 0
static uint32_t read_policy_configuration(db_t db_type,
					  struct policy_search_table *pst);
static void parse_line(char *line, struct policy_input **pi);
static void  build_attribute_structure(char *token,
				       struct policy_input *pi);
static void build_policy_structure(char *token,
		struct policy_input *pi);
static char * get_file_type(const char *fpath);
#endif
static void  add_policy_to_pst(const char *key, uint8_t index,
				  struct policy_entry *pe);
static int32_t get_pe_from_pi(struct policy_input *pi, struct policy_entry *pe);
static int32_t get_pdbe_from_pe(struct policy_entry *pe,
				struct policy_db_entry *pdbe);
static int32_t get_pe_from_pdbe(struct policy_db_entry *pdbe,
				struct policy_entry *pe);
static void  get_pe_from_tags(const char *tags[], size_t num_tags,
				struct policy_entry *pe);
static void policy_iterate(void *params, char *key,
			   void *data, ssize_t data_len);

/* ================ PRIVATE GLOBALS ==================*/

static struct policy_table policy_tab;
static struct policy_search_table pst;
static log_ctx_t *policy_ctx;
static uint8_t policy_initialized = 0;

/*=================Policy registration public APIs========================*/


/* Initialize the policy framework data structures */
void init_policy(db_t *db, db_type_t db_type, log_ctx_t *callee_ctx)
{
	int32_t i = 0;
	memset(&policy_tab, 0, sizeof(struct policy_table));
	pthread_rwlock_init(&policy_tab.pt_table_lock, NULL);
	policy_ctx = sfs_create_log_ctx();
	if (policy_ctx == NULL) {
		sfs_log(callee_ctx, SFS_ERR,
			"Unable to create policy log ctx"
			" policy wont be available");
		return;
	}
	policy_tab.policy_slots = (uint64_t) (-1);
	for (i = 0; i < NUM_BUCKETS; i++) {
		pst.pst_head[i] = (Pvoid_t)NULL;
		pthread_rwlock_init(&pst.pst_lock[i], NULL);
	}
	db->db_ops.db_iterate(db_type, policy_iterate, &pst);
	policy_initialized = TRUE;
	return;
}

int32_t submit_policy_entry(struct policy_input *pi, db_t *db,
			    db_type_t db_type)
{
	uint8_t index = 0;
	int rcint;
	int32_t ret = 0;
	struct policy_db_entry pdb_entry;
	struct policy_entry  *pe = NULL;

	char key[PATH_MAX + 16 /* UID + GID */
		+ TYPE_LEN + 4 /* \0 + 3 guard characters */];

	if (policy_initialized == FALSE || pi == NULL ||
	    db == NULL || db_type == 0) {
		return -EINVAL;
	}

	/* Allocate memory for policy_entry to be inserted
	   to the in-memory policy search table */
	if ((pe = malloc(sizeof(*pe) +pi->pi_num_policy *
			 sizeof(void *))) == NULL) {
		ret = -ENOMEM;
		goto ret;
	}
	if (pi->pi_ftype[0] != '*')
		index |= 1;
	if (pi->pi_gid != -1)
		index |= 2;
	if (pi->pi_uid != -1)
		index |= 4;
	if (pi->pi_fname[0] != '*')
		index |= 8;

	sprintf(key, "%s^%s^%d^%d", pi->pi_fname, pi->pi_ftype,
		pi->pi_uid, pi->pi_gid);

	pthread_rwlock_rdlock(&policy_tab.pt_table_lock);
	get_pe_from_pi(pi, pe);
	add_policy_to_pst(key, index, pe);
	get_pdbe_from_pe(pe, &pdb_entry);

	if (db->db_ops.db_insert(key, (char *)&pdb_entry,
				 sizeof(pdb_entry), db_type) == 0) {
		sfs_log(policy_ctx, SFS_INFO,
			"Policy submitted successfully");
	} else {
		sfs_log(policy_ctx, SFS_ERR, "Policy submit to db failed");
		/* Remove policy entry Judy array */
		JSLD(rcint, pst.pst_head[index], (uint8_t *)key);
		free(pe);
		ret = -EINVAL;
	}
	pthread_rwlock_unlock(&policy_tab.pt_table_lock);

ret:
	free(pi);
	return ret;
}

uint32_t register_plugin(const char *plugin_path, uint32_t *plugin_id)
{
	uint64_t slot;
	uint64_t temp;
	struct policy_plugin *plugin;

	if (policy_initialized == FALSE || plugin_path == NULL) {
		return -EINVAL;
	}

	plugin = malloc(sizeof(*plugin));
	if (!plugin) {
		sfs_log(policy_ctx, SFS_ERR,
			"Allocate memory for plugin failed: %s", plugin_path);
		return -ENOMEM;
	}
	memset(plugin, 0, sizeof(*plugin));
#if 0
	if (validate_plugin(plugin_path, plugin->pp_policy_name,
			    plugin->pp_sha_sum, policy_ctx) != 0) {
		sfs_log (policy_ctx, SFS_ERR,
			 "Plugin validation failed: %s", plugin_path);
		return -EINVAL;
	}
#endif
	pthread_spin_init(&plugin->pp_lock, PTHREAD_PROCESS_PRIVATE);
	pthread_rwlock_wrlock(&policy_tab.pt_table_lock);
	/* Look for an empty slot in the policy registration table */
	slot = ffs(policy_tab.policy_slots);
	policy_tab.pt_table[slot - 1] = plugin;
	temp = 1 << (slot - 1);
	policy_tab.policy_slots ^= temp;
	pthread_rwlock_unlock(&policy_tab.pt_table_lock);
	*plugin_id = (slot - 1);

	return 0;
}

uint32_t activate_plugin(uint32_t plugin_id)
{

	if (policy_initialized == FALSE) {
		return -EINVAL;
	}

	pthread_rwlock_rdlock(&policy_tab.pt_table_lock);
	pthread_spin_lock(&policy_tab.pt_table[plugin_id]->pp_lock);
	policy_tab.pt_table[plugin_id]->is_activated = TRUE;
	pthread_spin_unlock(&policy_tab.pt_table[plugin_id]->pp_lock);
	pthread_rwlock_unlock(&policy_tab.pt_table_lock);

	return 0;
}

uint32_t unregister_plugin(uint32_t plugin_id)
{
	struct policy_plugin *plugin;

	/* Remove references from the policy_tab table so that
	   the plugin can't be accessed any more by add_policy()
	   and friends */
	pthread_rwlock_wrlock(&policy_tab.pt_table_lock);
	plugin = policy_tab.pt_table[plugin_id];
	if (plugin) {
		policy_tab.pt_table[plugin_id] = NULL;
		policy_tab.policy_slots ^= 1 << (plugin_id + 1);
		pthread_rwlock_unlock(&policy_tab.pt_table_lock);
		/* if the reference count of the plugin is 0, remove it */
		if (plugin->pp_refcount == 0) {
			free(plugin);
		}
	}
	return 0;
}

/* =================== PRIVATE FUNCTIONS =========================== */

static void policy_iterate(void *params, char *key, void *data,
			   ssize_t data_len)
{
	struct policy_db_entry *pdbe;
	struct policy_entry *pe;

	if (params == NULL || key == NULL || data == NULL || data_len == 0) {
		sfs_log(policy_ctx, SFS_ERR, "%s(), line: %d: %s\n",
			__FUNCTION__, __LINE__, "Invalid Parameters");
		return;
	}

	pdbe = (struct policy_db_entry *)data;
	if ((pe = malloc(sizeof(*pe))) == NULL) {
		sfs_log(policy_ctx, SFS_ERR, "%s(): line %d, %s",
			__FUNCTION__, __LINE__, "pe allocate failed");
		return;
	}
	get_pe_from_pdbe(pdbe, pe);
	add_policy_to_pst(key, pdbe->pdb_mem_index, pe);
	return;
}

#if 0
static void parse_line(char *line, struct policy_input **policy_input)
{
	char *token = NULL;
	char *saveptr = NULL;
	uint32_t count = 0;
	struct policy_input *pi = NULL;
	/* Skip leading whitespaces if any */
	while (isspace(*line++));
	if (*(--line) == '#')
		return;

	if ((pi = malloc(sizeof(*pi))) == NULL)
		return;

	count = 0;
	token = strtok_r(line, " \t\n", &saveptr);
	while(token) {
		if (token[0] == '#')
			break;
		switch(count) {
			case 0:
				strcpy(pi->pi_fname, token);
				break;
			case 1:
				strcpy(pi->pi_ftype, token);
				break;
			case 2:
				if (token[0] == '*')
					pi->pi_uid = -1;
				else
					pi->pi_uid = atoi(token);
				break;
			case 3:
				if (token[0] == '*')
					pi->pi_gid = -1;
				else
					pi->pi_gid = atoi(token);
				break;
			case 4:
				build_attribute_structure(token, pi);
				break;
			case 5:
				build_policy_structure(token, pi);
				break;
		}
		count++;
		token = strtok_r(NULL, " \t\n", &saveptr);
	}
	if (pi->pi_num_policy == 0 &&
			pi->pi_attr == NULL) {
		/* Its a commented line of no use to us */
		free(pi);
		*policy_input = NULL;
	} else {
		*policy_input = pi;
	}
	return;
}


static void build_attribute_structure(char *string,
		struct policy_input *policy_input)
{
	char *token = NULL;
	char *saveptr = NULL;
	struct attribute *attribute = NULL;

	token = strtok_r(string, ",\t", &saveptr);
	while(isspace(*token++));
	if (*(--token) == '#')
		return;
	if ((attribute = malloc(sizeof(*attribute))) == NULL)
			return;
	memset(attribute, 0, sizeof(*attribute));

	while (token) {
		if (!strcmp(token, "hidden"))
			attribute->a_ishidden = 1;
		else if (!strcmp(token, "qos-high"))
			attribute->a_qoslevel = 1;
		else if (!strcmp(token, "qos-low"))
			attribute->a_qoslevel = 0;
		token = strtok_r(NULL, ",\t", &saveptr);
	}
	policy_input->pi_attr = attribute;
	return;
}

static void build_policy_structure(char *string,
		struct policy_input *pi)
{
	char *token = NULL;
	char *saveptr = NULL;
	uint32_t i = 0;

	token = strtok_r(string, ",\t\n ", &saveptr);
	while (isspace(*token++));
	if (*(--token) == '#')
		return;

	while (token && (i < NUM_MAX_POLICY)) {
		/* Iterate through registered policies to see if we have
		   the plugin with us */
		strncpy(pi->pi_policy_tag[i], token, POLICY_TAG_LEN);
		pi->pi_num_policy++;
		token = strtok_r(NULL, ",\t\n", &saveptr);
		i++;
	}
	return;
}
#endif
static int32_t get_pdbe_from_pe(struct policy_entry *pe,
				    struct policy_db_entry *pdbe)
{
	int32_t i, ret = 0;

	if (pe == NULL || pdbe == NULL) {
		sfs_log(policy_ctx, SFS_ERR, "%s(): line: %d: %s",
			__FUNCTION__, __LINE__,
			"Invalid parameters specified");
		ret = -EINVAL;
		goto ret;
	}

	/* Acquire the policy entry spin lock */
	pthread_spin_lock(&pe->pe_lock);
	for(i = 0; i < pe->pe_num_plugins; ++i) {
		strncpy(pdbe->pdb_policy_tag[i],
			pe->pe_policy[i]->pp_policy_name, POLICY_TAG_LEN);
		memcpy(pdbe->pdb_policy_cksum[i],
		       pe->pe_policy[i]->pp_sha_sum, SHA256_DIGEST_LENGTH);
	}
	pdbe->pdb_num_policy = pe->pe_num_plugins;
	memcpy(&pdbe->pdb_attr, &pe->pe_attr, sizeof(*pdbe));
	pthread_spin_unlock(&pe->pe_lock);
ret:

	return ret;
}
/* pe must be allocated while calling this */
static int32_t get_pe_from_pdbe(struct policy_db_entry *pdb,
				     struct policy_entry *pe)
{
	int32_t ret = 0;

	if (pdb == NULL || pe == NULL) {
		sfs_log(policy_ctx, SFS_ERR, "%s(): line : %d: %s",
			__FUNCTION__, __LINE__,
			"Invalid parameters specified");
		ret = -EINVAL;
		goto ret;
	}
	pthread_spin_init(&pe->pe_lock, PTHREAD_PROCESS_PRIVATE);
	memcpy(&pe->pe_attr, &pdb->pdb_attr, sizeof(struct attribute));
	get_pe_from_tags((const char **)pdb->pdb_policy_tag,
			pdb->pdb_num_policy, pe);

ret:
	return ret;
}

/* pe must be allocated while calling this */
static inline int32_t get_pe_from_pi(struct policy_input *pi,
					  struct policy_entry *pe)
{
	int32_t ret = 0;

	if (pi == NULL || pe == NULL) {
		sfs_log (policy_ctx, SFS_ERR, "%s(): line %d: %s",
			 __FUNCTION__, __LINE__,
			 "Invalid parameters specified");
		ret = -EINVAL;
		goto ret;
	}
	pthread_spin_init(&pe->pe_lock, PTHREAD_PROCESS_PRIVATE);
	memcpy(&pe->pe_attr, &pi->pi_attr, sizeof(struct attribute));
	get_pe_from_tags((const char **)pi->pi_policy_tag,
			 pi->pi_num_policy, pe);

ret:
	return ret;

}
/*
 * policy_tab->pt_table_lock must be held while calling this!!
 */
static void  get_pe_from_tags(const char *policy_tags[],
				     size_t num_tags,
				     struct policy_entry *pe)
{
	int32_t i, j, k;
	int32_t filled_policies = 0;
	struct policy_plugin *pp = NULL;

	for(i = 0, k = 0; i < num_tags; i++) {
		for(j = 0; j < NUM_MAX_POLICY; j++) {
			if (policy_tab.pt_table[j] == NULL)
				continue;
			if (!strcmp(policy_tags[i],
				policy_tab.pt_table[j]->pp_policy_name)) {
				pp = policy_tab.pt_table[j];
				pe->pe_policy[k++] = pp;
				pe->pe_num_plugins++;
				pthread_spin_lock(&pp->pp_lock);
				pp->pp_refcount++;
				pthread_spin_unlock(&pp->pp_lock);
			}
		}
	}
	return;
}

/* Add policy into the Judy array -
 * policy_tab->pt_table_lock must be held while
 * calling this!!
 */
static void add_policy_to_pst(const char *key, uint8_t index,
		       struct policy_entry *pe)
{
	Word_t	*pvalue;
	/* Insert the policy_entry into the Judy array */
	pthread_rwlock_wrlock(&pst.pst_lock[index]);
	JSLI(pvalue, pst.pst_head[index], (uint8_t *)key);
	pthread_rwlock_unlock(&pst.pst_lock[index]);
	if (pvalue)
		*pvalue = (Word_t) pe;
	return;
}

/* TODO: Change the implementation */
static char *get_file_type(const char *fpath)
{
	char command[PATH_MAX + 6];
	FILE *fp = NULL;
	char *ftype = NULL;
	char *saveptr;
	sprintf (command, "file %s", fpath);
	if ((fp = popen(command, "r")) == NULL)
		return "*";
	if (!fgets(command, PATH_MAX, fp)) {
		pclose(fp);
		return "*";
	}
	ftype = strtok_r(command, ": \t", &saveptr);
	ftype = strtok_r(NULL, ":", &saveptr);
	printf ("%s() : File type: %s\n", __FUNCTION__, ftype);
	if (strstr(ftype, "text"))
		ftype = "text";
	return ftype;
}

/* ==================== POLICY ACCESS PUBLIC APIs ====================*/
struct policy_entry* get_policy(const char* path)
{
	int32_t i;
	struct policy_entry *pe = NULL;
	Word_t *pvalue;
	/* Index variables. Helps in
	   creating Judy indexes */
	char *fname[2], *index_fname;
	char *ftype[2], *index_ftype;
	uid_t uid[2], index_uid;
	gid_t gid[2], index_gid;
	/* Actual Judy index holder */
	char index[PATH_MAX + 16 /* UID + GID */
		+ TYPE_LEN + 4 /* '\0' and guard characters */ ];

	/* UID */
	uid[0] = -1;
	uid[1] = getuid();
	/* GID */
	gid[0] = -1;
	gid[1] = getgid();

	/* File Type */
	ftype[0] = "*";
	ftype[1] = get_file_type(path);
	printf ("File type fetched: %s\n", ftype[1]);

	/* File Name */
	fname[0] = "*";
	fname[1] = (char*)path;

	/* Search from the specific to the generic policies */
	for(i = (NUM_BUCKETS - 1); (i >= 0) && (pe == NULL); i--) {
		/* Generate the Judy index here depending
		   upon the number of bits set in the
		   bucket number */
		index_fname = fname[(i >> 3) & 1];
		index_uid = uid[(i >> 2) & 1];
		index_gid = gid[(i >> 1) & 1];
		index_ftype = ftype[(i & 1)];
		sprintf (index, "%s^%x^%x^%s", index_fname,index_uid,
				index_gid, index_ftype);
		pthread_rwlock_rdlock(&pst.pst_lock[i]);
		JSLG(pvalue, pst.pst_head[i], (uint8_t *)index);
		if (pvalue == NULL) {
			pthread_rwlock_unlock(&pst.pst_lock[i]);
			continue;
		}
		pe = (struct policy_entry *)(*pvalue);
		pthread_spin_lock(&pe->pe_lock);
		pe->pe_refcount++;
		pthread_spin_unlock(&pe->pe_lock);
		pthread_rwlock_unlock(&pst.pst_lock[i]);
	}
	return pe;
}

#ifdef POLICY_TEST
int main(void)
{
	struct policy_plugin plugin[3];
	int plugin_id[3];
	int i = 0;
	struct policy_entry *pe = NULL;
	init_policy();
#define NUM_PLUGIN 3
	/*plugin[0] - Encryption */
	strcpy(plugin[0].pp_policy_name, "encryption");
	strcpy(plugin[1].pp_policy_name, "plugin1");
	strcpy(plugin[2].pp_policy_name, "plugin2");

	for(i=0; i < NUM_PLUGIN; i++) {
		pthread_spin_init(&plugin[i].pp_lock,
				PTHREAD_PROCESS_PRIVATE);
		register_plugin(&plugin[i], &plugin_id[i]);
		printf ("Plugin (%d) registered at : %d (id)\n",
				i, plugin_id[i]);
	}
	printf ("policy return: %d\n", read_policy_configuration());
	printf ("============FETCH DETAILS================\n");
	pe = get_policy("/home/shubhro/policy/policy.c");
	if (pe != NULL) {
		for (i = 0; i < pe->pe_num_plugins; ++i)
			printf ("policy tag: %s\n",
					pe->pe_policy[i]->pp_policy_name);
	} else {
		printf ("No policy found\n");
	}
	return 0;
}
#endif
