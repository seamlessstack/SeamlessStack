#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <Judy.h>
#include "policy.h"
#include <pthread.h>

#define _GNU_SOURCE

#define POLICY_TEST
#define NUM_BUCKETS 16
#define MAX_POLICIES 64
#define TYPE_LEN 8

#define POLICY_FILE "/home/shubhro/policy/policy.conf"
/*=================== PRIVATE STRUCTURES AND FUNCTIONS ===================*/
struct policy_input
{
	uid_t	pi_uid;
	gid_t	pi_gid;
	char	pi_policy_tag[MAX_POLICIES][POLICY_TAG_LEN];
	char	pi_ftype [TYPE_LEN];
	char	pi_fname[PATH_MAX];
	size_t	pi_num_policy;
	struct attribute *pi_attr;
};

static uint32_t read_policy_configuration(void);
static void parse_line(char *line, struct policy_input **pi);
static uint32_t add_policy(struct policy_input *input); 
static void  build_attribute_structure(char *token,
		struct policy_input *pi);
static void build_policy_structure(char *token,
		struct policy_input *pi);
static char * get_file_type(const char *fpath);

/*=================Policy registration public APIs========================*/

static struct policy_table policy_tab;
static struct policy_search_table pst;

/* Initialize the policy framework data structures */
void init_policy(void)
{
	int32_t i = 0;
	memset(&policy_tab, 0, sizeof(struct policy_table));
	pthread_rwlock_init(&policy_tab.pt_table_lock, NULL);
	policy_tab.policy_slots = (uint64_t) (-1);
	for (i = 0; i < NUM_BUCKETS; i++) {
		pst.pst_head[i] = (Pvoid_t)NULL;
		pthread_rwlock_init(&pst.pst_lock[i], NULL);
	}
	return;
}

uint32_t register_plugin(struct policy_plugin *policy,
		uint32_t *plugin_id)
{
	uint64_t slot;
	uint64_t temp;
	if (policy == NULL) {
		printf ("Invalid policy supplied");
		return -EINVAL;
	}
	pthread_rwlock_wrlock(&policy_tab.pt_table_lock);
	/* Look for an empty slot in the policy registration table */
	slot = ffs(policy_tab.policy_slots);
	policy_tab.pt_table[slot - 1] = policy;
	temp = 1 << (slot - 1);
	policy_tab.policy_slots ^= temp;
	pthread_rwlock_unlock(&policy_tab.pt_table_lock);
	*plugin_id = (slot - 1);

	return 0;
}

uint32_t activate_plugin(uint32_t plugin_id)
{
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
	policy_tab.pt_table[plugin_id] = NULL;
	policy_tab.policy_slots ^= 1 << (plugin_id + 1);
	pthread_rwlock_unlock(&policy_tab.pt_table_lock);
	/* if the reference count of the plugin is 0, remove it */
	pthread_spin_lock(&plugin->pp_lock);
	if (plugin->pp_refcount == 0) {
		free(plugin);
	}
	pthread_spin_unlock(&plugin->pp_lock);
	return 0;
}

/* =================== PRIVATE FUNCTIONS =========================== */

/* Read the policy configuration and call functions
 * to add/associate policy with the files 
 */  
static uint32_t read_policy_configuration(void)
{	
	FILE *fp = NULL;
	char *line = NULL;
	ssize_t len = 0;
	struct policy_input *pi = NULL;

	if ((fp = fopen(POLICY_FILE, "r")) == NULL) {
		perror("Unable to open policy file");
		return -errno;
	}
	
	while (getline(&line, &len, fp) > 0) {
		parse_line(line, &pi);
		if (pi != NULL)
			add_policy(pi);
	}
	free(line);
	return EXIT_SUCCESS;
}


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
		else if (!strcmp(token, "striped"))
			attribute->a_isstriped = 1;
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
	struct policy_input *policy_input = NULL;
	uint32_t i = 0;	
	uint32_t j = 0;	
	
	token = strtok_r(string, ",\t\n ", &saveptr);
	while (isspace(*token++));
	if (*(--token) == '#')
		return;

	while (token && (i < MAX_POLICIES)) {
		/* Iterate through registered policies to see if we have
		   the plugin with us */
		strncpy(pi->pi_policy_tag[i], token, POLICY_TAG_LEN);
		pi->pi_num_policy++;
		token = strtok_r(NULL, ",\t\n", &saveptr);
		i++;
	}
	return;
}

static uint32_t add_policy(struct policy_input *pi)
{
	int32_t i = 0, j = 0, k = 0;
	int32_t index = 0, filled_policies = 0;
	char    buf[PATH_MAX + 16 /* UID + GID */
		+ TYPE_LEN + 4 /* \0 + 3 guard characters */]; 
	struct  policy_entry *pe = NULL;
	struct	policy_plugin *pp;
	Word_t	*pvalue;
	/* Apply the precedence rules here */
	if (pi->pi_ftype[0] != '*')
		index |= 1;
	if (pi->pi_gid != -1)
		index |= 2; 
	if (pi->pi_uid != -1)
		index |= 4;
	if (pi->pi_fname[0] != '*')
		index |= 8;
	
	sprintf(buf, "%s^%x^%x^%s", pi->pi_fname,
			pi->pi_uid, pi->pi_gid, pi->pi_ftype);
	/* Allocate memory for policy_entry to be inserted */
	if ((pe = malloc(sizeof(*pe) +pi->pi_num_policy * 
					sizeof(void *))) == NULL)
		return -ENOMEM;
	pthread_spin_init(&pe->pe_lock, PTHREAD_PROCESS_PRIVATE);
	/* Find the policy to be applied */
	pthread_rwlock_rdlock(&policy_tab.pt_table_lock);
	filled_policies = __builtin_popcount(~(policy_tab.policy_slots));
	for(i = 0, k = 0; i < pi->pi_num_policy; i++) {
		for(j = 0; j < filled_policies; j++) { 
			if (!strcmp(pi->pi_policy_tag[i],
				policy_tab.pt_table[j]->pp_policy_tag)) {
				pp = policy_tab.pt_table[j];
				pe->pe_policy[k++] = pp;
				pe->pe_num_plugins++;
				pthread_spin_lock(&pp->pp_lock);
				pp->pp_refcount++;
				pthread_spin_unlock(&pp->pp_lock);
			}
		}
	}
	pthread_rwlock_unlock(&policy_tab.pt_table_lock);
	pe->pe_attr = pi->pi_attr;
	/* Insert the policy_entry into the Judy array */
	pthread_rwlock_wrlock(&pst.pst_lock[index]);
	JSLI(pvalue, pst.pst_head[index], buf);
	pthread_rwlock_unlock(&pst.pst_lock[index]);
	if (pvalue)
		*pvalue = (Word_t) pe;
	return 0;
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
	int reverse_index;
	
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
		JSLG(pvalue, pst.pst_head[i], index);
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
	strcpy(plugin[0].pp_policy_tag, "encryption"); 
	strcpy(plugin[1].pp_policy_tag, "plugin1"); 
	strcpy(plugin[2].pp_policy_tag, "plugin2");

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
					pe->pe_policy[i]->pp_policy_tag);
	} else {
		printf ("No policy found\n");
	}
	return 0;
}
#endif
