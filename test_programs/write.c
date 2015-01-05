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
#include <fcntl.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
	int fd = 0;
	int nbytes = 0;
	if (argc == 1) {
		printf ("Error in use\n");
		return -1;
	}

	fd = open(argv[1], O_CREAT|O_WRONLY, S_IRWXU);

	if (fd > 0) {
		nbytes = write(fd, "12345", 6);
		printf ("Wrote %d bytes\n", nbytes);
		perror("write status");
		close(fd);
	}
#if 1
	fd = open(argv[1], O_RDONLY);
	printf ("file des: %d\n", fd);
	perror("open status");

	if (fd > 0) {
		char buf[10];
		nbytes = read(fd, buf, 10);
		perror("Read status");
		printf ("Read %d bytes, data = %s\n", nbytes, buf);
	}
#endif
	return 0;
}

