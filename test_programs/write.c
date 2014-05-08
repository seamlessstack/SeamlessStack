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

	fd = open(argv[1], O_WRONLY);

	printf ("Open fd = %d\n", fd);

	if (fd > 0) {
		nbytes = write(fd, "12345", 6);
		printf ("Wrote %d bytes\n", nbytes);
		perror("write status");
	}

	return 0;
}

