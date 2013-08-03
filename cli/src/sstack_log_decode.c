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


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sstack_log.h>

void
usage(char *progname)
{
	fprintf(stderr, "Usage: %s file_name\n", progname);
	exit(-1);
}


int
main(int argc, char *argv[])
{
	int fd = -1;
	log_entry_t log_entry = { '\0' };
	int nbytes = -1;

	// Arg check
	if (argc != 2)
		usage(argv[0]);

	fd = open(argv[1], O_RDONLY);
	ASSERT((fd != -1), "Open failed. Exiting...", 0, 1, -1);
	nbytes = read(fd, &log_entry, sizeof(log_entry_t));
	if (nbytes < sizeof(log_entry_t)) {
		fprintf(stderr, "%s: Not enough bytes to read and interpret.\n",
			argv[0]);
		close(fd);
		exit(-1);
	}

	while (nbytes == sizeof(log_entry_t)) {
		// Print the log entry
		fprintf(stdout,"%d:%s:%s:%s\n", log_entry.version,
			log_entry.time, log_entry.level, log_entry.log);
		nbytes = read(fd, &log_entry, sizeof(log_entry_t));
	}

	if ((nbytes < sizeof(log_entry_t)) &&
			(nbytes != 0)) {
		fprintf(stderr, "%s: Not enough bytes to read and interpret.\n",
			argv[0]);
		close(fd);
		exit(-1);
	} else if (nbytes == 0) {
		// File read successful
		close(fd);
	}

	return 0;
}
