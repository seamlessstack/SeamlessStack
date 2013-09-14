#ifndef __POLICY_H__
#define __POLICY_H__

#define NUM_BUCKETS 16
#define POLICY_TAG_LEN 16
#define NUM_MAX_POLICY 64

#include <Judy.h>
#define SHA256_DIGEST_LENGTH 32

enum ret_code
{
	FALSE,
	TRUE,
};

/**
  * Represents the supported attributes.
  * Would be allocated one per file 
 **/
struct attribute
{
	uint32_t ver;
	uint8_t a_qoslevel;
	uint8_t	a_ishidden:1;
};

/**
  * Represents one plugin. Allocated during 
  * registration.
 **/ 

typedef struct policy_plugin policy_plugin_t;

typedef struct policy_plugin
{
	uint32_t ver;
	uint32_t pp_refcount;
	char	 pp_policy_name[POLICY_TAG_LEN];
	uint8_t  pp_sha_sum[SHA256_DIGEST_LENGTH];
	uint8_t  is_activated;
	pthread_spinlock_t pp_lock;
} policy_plugin_t;

/** Container to keep attributes and plugins
  * together per file. Result of a get_policy()
  * call
 **/ 
struct policy_entry
{
	struct attribute *pe_attr;
	size_t pe_num_plugins;
	uint32_t pe_refcount;
	pthread_spinlock_t pe_lock;
	struct policy_plugin *pe_policy[0];
};


/**
  * The global policy table. Keeps
  * the registered plugins at one 
  * place. Accessed when a plugin is 
  * associated with a file
  **/
struct policy_table
{
	pthread_rwlock_t pt_table_lock;
	struct policy_plugin *pt_table[NUM_MAX_POLICY];
	uint64_t policy_slots;
};

/**
  * Table which maps file -> plugin/attribute
  * association
**/
struct policy_search_table
{
	/* initialize NUM_BUCKET Judy arrays */
	Pvoid_t pst_head[NUM_BUCKETS];
	pthread_rwlock_t pst_lock[NUM_BUCKETS];
};


/* Policy framework functions */
uint32_t register_plugin(struct policy_plugin *plugin, 
		uint32_t *plugin_id);
uint32_t unregister_plugin(uint32_t plugin_id);
uint32_t activate_plugin(uint32_t plugin_id);

/* Get policy, to be called from read/write ops */
struct policy_entry* get_policy(const char *path);

/* Configuration , get, set routines */
#endif /* __POLICY_H__ */
