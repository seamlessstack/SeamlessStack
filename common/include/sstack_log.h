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

#ifndef __SSTACK_LOG_H_
#define __SSTACK_LOG_H_
#include <fcntl.h>
#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>


#define SFS_LOG_DIRECTORY "/home/shubhro/sfsd_dir"
#define SFS_LOGFILE_LEN 32
#define SFS_TIME_FORMAT_LEN 64
#define SFS_LOGLEVEL_LEN 16
#define SFS_LOG_ENTRY_LEN 128
#define LOG_ENTRY_VERSION 1

extern char sstack_log_directory[];

/*
 * ASSERT takes three arguments:
 * p - a condition 
 * q - a string
 * r - assert or not
 * s -  exit out of the function? 1 for yes and 0 for no
 * t - return value if r = 0 and s = 1 . Otherwise ignored
 */
 
#define ASSERT(p, q, r, s, t)	do { \
	if (!(p))  \
	fprintf(stderr, "%s: %s\n", __FUNCTION__, (q));\
	if ((r) == 1) \
	assert((p)); \
	else { \
		if ((s) != 0 && !p) \
		return (t); \
	} \
} while(0);

typedef enum {
	SFS_EMERG	= 1,
	SFS_CRIT	= 2,
	SFS_ERR		= 3,
	SFS_WARNING = 4,
	SFS_INFO	= 5,
	SFS_DEBUG	= 6,
	MAX_SFS_LOG_LEVELS = 6,
} sfs_log_level_t;

typedef struct log_context
{
	pthread_mutex_t log_mutex; // For synchronization
	int log_fd;  // FD of the currently used log file
	FILE *log_fp;
	char *log_file_name; // log file name current being used
	// TODO
	// Place holder for older log files.
	// We can use syslog-like file name semantics too.
	int log_initialized; // self explanatory
	sfs_log_level_t log_level; // Current logging level
} log_ctx_t;


typedef struct log_entry
{
	int	 version;
	char time[SFS_TIME_FORMAT_LEN];
	char level[SFS_LOGLEVEL_LEN];
	char log[SFS_LOG_ENTRY_LEN];
} log_entry_t;

/*
 * sfs_create_log_ctx - Create log context
 *
 * Returns allocated log ctx or NULL
 */

static inline log_ctx_t *
sfs_create_log_ctx(void)
{
	log_ctx_t *ctx = NULL;
	ctx = malloc(sizeof(log_ctx_t));
	ASSERT((ctx != NULL), "Unable to allocate memory for ctx", 0, 1, NULL);
	pthread_mutex_init(&ctx->log_mutex, NULL);
	ctx->log_fd = -1;
	ctx->log_file_name = (char *) malloc(PATH_MAX);
	ASSERT((ctx->log_file_name != NULL),
		"Unable to allocate memory for ctx->log_file_name", 0, 0, NULL);
	if (NULL == ctx->log_file_name) {
		free(ctx);
		return NULL;
	}
	ctx->log_initialized = -1;
	ctx->log_level =  SFS_DEBUG;

	return ctx;
}


/*
 * sfs_destroy_log_ctx - destroy the log context
 */

static inline int
sfs_destroy_log_ctx(log_ctx_t *ctx)
{
	int ret = -1;

	ASSERT((ctx != NULL), "ctx parameter is NULL", 0, 1, -1);
	// pthread_mutex_destroy doesn't free any resources on Linux
	// It just checks if mutex is locked. If so, EBUSY is returned.
	ret = pthread_mutex_destroy(&ctx->log_mutex);
	ASSERT((ret != EBUSY), "Mutex is locked !!", 0, 1, -1);
	ctx->log_fd = -1;
	ASSERT((ctx->log_file_name != NULL),
		"log_file_name is already freed !! ", 0, 1, -1);
	if (ctx->log_file_name)
		free(ctx->log_file_name);

	free(ctx);
}

/*
 * sfs_log_close - Close the log file related to a log_ctx
 *
 * ctx is the log context, It is checked for non-NULL only
 * 
 * All that is done is to close the log_fd in the ctx
 * Returns 0 on success and -1 if something is horribly wrong
 * Does not use assert() as failure is non-critical.
 */

static inline int
sfs_log_close(log_ctx_t *ctx)
{
	ASSERT((ctx != NULL), "Ctx is NULL", 0, 1, -1);

	close(ctx->log_fd);

	return 0;
}

/*
 * sfs_change_loglevel - Change current log level settings in a log ctx
 *
 * ctx is the log context
 * level is the new log level
 *
 * Returns 0 on success
 * Returns -1 on failure
 */

static inline int
sfs_change_loglevel(log_ctx_t *ctx, sfs_log_level_t level)
{
	ASSERT((ctx != NULL), "Log context is NULL.", 0, 1, -1);
	ASSERT((level > 0 && level < MAX_SFS_LOG_LEVELS),
		"Invalid log level specified.", 0, 1, -1);
	pthread_mutex_lock(&ctx->log_mutex);
	ctx->log_level = level;
	pthread_mutex_unlock(&ctx->log_mutex);

	return 0;
}

/*
 * sfs_log_init - Initialize file based logging for SeamlessStack
 *
 * This is written to satisy file based logging for all components 
 *
 * Parameter assumptions:
 * ctx is allocated
 * level is one of sfs_log_level_t
 * compname is caller's name. This is used for creating log file name.
 * Checks if user has specified any log directory. If so, uses that directory.
 *
 */

static inline int
sfs_log_init(log_ctx_t *ctx, sfs_log_level_t level, char *compname)
{

	char log_file_abspath[PATH_MAX] = { '\0' };
	char file_name[SFS_LOGFILE_LEN] = { '\0' };
	int fd = -1;

	ASSERT((ctx != NULL), "Context not allocated.", 0, 1, -1);
	ASSERT((level > 0 && level < MAX_SFS_LOG_LEVELS),
		"Invalid log level specified.", 0, 1, -1);
	ASSERT((compname[0] != '\0'), "Progname is NULL.", 0, 1, -1);

	// Since this operation is not done often, 
	pthread_mutex_lock(&ctx->log_mutex);
	strncpy(file_name, compname, 27);
	// Check if user specified a log directory
	// Check if that directory exists and writable
	if (access(sstack_log_directory, W_OK) == 0) {
		sprintf(log_file_abspath, "%s/%s_%ld%s", sstack_log_directory,
				file_name, time(NULL), ".log");
	} else {
		// Go ahead and use default SFS directory
		// Check if SFS_LOG_DIRECTORY exists
		if (access(SFS_LOG_DIRECTORY, W_OK) != 0) {
			ASSERT((1 == 0), "Log directory not created. Logging disabled",
				0, 1, -1);
	 	}
		sprintf(log_file_abspath, "%s/%s_%ld%s", SFS_LOG_DIRECTORY,
				file_name, time(NULL), ".log");
	}

	// TODO
	// Need to study the effect of O_DIRECT|O_SYNC
	// They are desired in worst case scenario(system down)
	fd = open(log_file_abspath, O_CREAT|O_RDWR|O_LARGEFILE,
			S_IRUSR|S_IWUSR);
	if (fd == -1) {
		// Failed to open log file 
		fprintf(stderr, "%s: Failed to open log file %s."
			" Logging is disabled\n", __FUNCTION__, log_file_abspath);
		ctx->log_fd = -1;
		ctx->log_initialized = 0;
		pthread_mutex_unlock(&ctx->log_mutex);
		return -1;
	} else {
		// Populate the log ctx.
		ctx->log_fd = fd;
		ctx->log_initialized = 1;
		ctx->log_level = level;
		strcpy(ctx->log_file_name, log_file_abspath);
		/* TODO: Remove after log decode utility is done */
		ctx->log_fp = fdopen(fd,"a");
		pthread_mutex_unlock(&ctx->log_mutex);
	}


	return 0;	
}

/*
 * sfs_log - Log the message to log file
 *
 * ctx is the logging context for the component
 * level is the log level for this log entry
 * format is the format string
 *
 * Returns 0 on success, -1 on failure. Returns 1 if loging
 * is not done due to current log level set lower than the
 * log level of this log entry.
 */

static inline int
sfs_log(log_ctx_t *ctx, sfs_log_level_t level, char *format, ...)
{
	va_list list;
	int res = -1;
	time_t t = time(0);
	struct tm *lt = NULL;
	log_entry_t log_entry;

	ASSERT((ctx != NULL), "ctx is NULL. Can not log", 0, 1, -1);
	ASSERT((level > 0 && level < MAX_SFS_LOG_LEVELS),
		"Invalid log level specified", 0, 1, -1);
	ASSERT((format != NULL), "Format string is NULL.", 0, 1, -1);
	if (level > ctx->log_level) {
		// Cuurent log level is set higher.
		// This message is ignored.
		return 1;
	}

	memset(&log_entry, '\0', sizeof(log_entry_t));
	log_entry.version = LOG_ENTRY_VERSION;
	lt = localtime(&t);
	strftime((char * __restrict__) &(log_entry.time), SFS_TIME_FORMAT_LEN,
			"%Y-%m-%d %H:%M:%S", lt);


	switch(level) {
		case SFS_EMERG:
			snprintf((char * __restrict__) &log_entry.level,
				SFS_LOGLEVEL_LEN, "%s: ", "EMERGENCY");
			break;
		case SFS_CRIT:
			snprintf((char * __restrict__) &log_entry.level,
				SFS_LOGLEVEL_LEN, "%s: ", "CRITICAL");
			break;
		case SFS_ERR:
			snprintf((char * __restrict__) &log_entry.level,
				SFS_LOGLEVEL_LEN, "%s: ", "ERROR");
			break;
		case SFS_WARNING:
			snprintf((char * __restrict__) &log_entry.level,
				SFS_LOGLEVEL_LEN, "%s: ", "warning");
			break;
		case SFS_INFO:
			snprintf((char * __restrict__) &log_entry.level,
				SFS_LOGLEVEL_LEN, "%s: ", "info");
			break;
		case SFS_DEBUG:
			snprintf((char * __restrict__) &log_entry.level,
				SFS_LOGLEVEL_LEN, "%s: ", "debug");
			break;
	}

	va_start(list, format);
	res = vsprintf((char * __restrict__) &log_entry.log,
			(char * __restrict__) format, list);
	va_end(list);
	pthread_mutex_lock(&ctx->log_mutex);
#if 1
	// Go ahead and write to the log file
	res = write(ctx->log_fd, &log_entry, sizeof(log_entry_t));
#else
	/* Till the time decoding utility is done,
	   dump raw text */
	res = fprintf(ctx->log_fp, "%s == %s == %s \n", log_entry.time,
			log_entry.level, log_entry.log);
#endif // if 1
	pthread_mutex_unlock(&ctx->log_mutex);
	// Not sure whether fclose(out) will cause close(ctx->log_fd)
	// TBD
	ASSERT((res != 0),
		"Failed to write to log file", 0, 1, -1);

	return 0;
}

/*
 * sfs_print_logs_by_level - Print logs of particular log level
 * TBD
 */

 /* sfs_print_logs_by_time - Print logs between two times given in localtime 
 * format and by priority(6 for all logs)
 * TBD
 */


#endif //__SSTACK_LOG_H_
