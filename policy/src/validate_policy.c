/*
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

#include <stdint.h>
#include <sstack_log.h>
#include <dlfcn.h>
#include <policy.h>

/* out_name must be allocated here */
int32_t validate_plugin(const char *plugin_path, struct plugin_entry_points
						*entry, char *out_name, log_ctx_t *ctx)
{
	void *handle;
	char *plugin_name = NULL;
	char local_plugin_name[PATH_MAX];
	char plugin_prefix[32];
	char *start, *end;

	if (NULL == out_name)
		return -EFAULT;

	/* Verify if the plugin conforms to the criterion
	   provided by us */
	handle = dlopen(plugin_path, RTLD_LAZY);
	if (!handle) {
		sfs_log(ctx, SFS_ERR, "%s", dlerror());
		return -EINVAL;
	}
	/* So the file is a valid dynamic library lets check
	   for the necessary functions */
	strcpy(local_plugin_name, plugin_path);
	plugin_name = basename(local_plugin_name);
	start = strstr(plugin_name, "lib");
	end = strstr(plugin_name, ".so");
	if ((start != plugin_name) || (end == NULL) || ((start + 3) >= end )) {
		sfs_log(ctx, SFS_ERR, "Invalid plugin name");
		return -EINVAL;
	} else {
		char function_name[64];
		start = start + 3; /* ignore the string "lib" from the
					 plugin lib name */
		strncpy(plugin_prefix, start, (end - start)/sizeof(char));
		/* Check the *_init, *_deinit, *_apply, *_remove functions */
		sprintf (function_name, "%s_init", plugin_prefix);
		entry->init = dlsym(handle, function_name);
		sprintf(function_name, "%s_deinit", plugin_prefix);
		entry->deinit = dlsym(handle, function_name);
		sprintf(function_name, "%s_apply", plugin_prefix);
		entry->apply = dlsym(handle, function_name);
		sprintf(function_name, "%s_remove", plugin_prefix);
		entry->remove = dlsym(handle, function_name);
		if (entry->init == NULL || entry->deinit == NULL ||
				entry->apply == NULL || entry->remove == NULL)
		{
			sfs_log(ctx, SFS_ERR, "Invalid Plugin");
			return -EINVAL;
		}
		strncpy(out_name, plugin_prefix, (end - start)/sizeof(char));
	}
	return 0;
}
