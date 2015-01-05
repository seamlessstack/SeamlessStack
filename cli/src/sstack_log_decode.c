/*
 * Copyright (C) 2014 SeamlessStack
 *
 *  This file is part of SeamlessStack distributed file system.
 *
 * SeamlessStack distributed file system is free software: you can redistribute
 * it and/or modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 *
 * SeamlessStack distributed file system is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SeamlessStack distributed file system. If not,
 * see <http://www.gnu.org/licenses/>.
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
		fprintf(stdout,"%s\t%s\t%s",
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
