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
int32_t validate_plugin(const char *plugin_path, log_ctx_t *ctx)
{
	void *handle;
	void *function[4];
	char *plugin_name = NULL;
	char local_plugin_name[PATH_MAX];
	char plugin_prefix[32];
	char *start, *end;
	/* Verify if the plugin conforms to the criterion
	   provided by us */
	handle = dlopen(plugin_path, RTLD_LAZY);
	if (!handle) {
		sfs_log(log_ctx, SFS_ERR, "%s", dlerror());
		return -EINVAL;
	}
	/* So the file is a valid dynamic library lets check
	   for the necessary functions */
	strcpy(local_plugin_name, plugin_path);
	plugin_name = basename(local_plugin_path);
	start = strstr(plugin_name, "lib");
	end = strstr(plugin_name, ".so");
	if ((start == NULL) || (end == NULL) || (start >= end )) {
		sfs_log(ctx, SFR_ERR, "Invalid plugin name");
		return -EINVAL;
	} else {
		char function_name[64];
		int32_t i = 0;
		strncpy(plugin_prefix, start, (end - start)/sizeof(char));
		/* Check the *_init, *_deinit, *_apply, *_remove functions */
		sprintf (function_name, "%s_init", plugin_prefix);
		function[0] = dlsym(handle, function_name);
		sprintf(function_name, "%s_deinit", plugin_prefix);
		function[1] = dlsym(handle, function_name);
		sprintf(function_name, "%s_apply", plugin_prefix);
		function[2] = dlsym(handle, function_name);
		sprintf(function_name, "%s_remove", plugin_prefix);
		function[3] = dlsym(handle, function_name);
		for(i = 0; i < 4; ++i) {
			if (function[i] == NULL) {
				sfs_log(log_ctx,
					SFS_ERR, "Invalid function name");
			}
		}
	}
	return 0;
}
