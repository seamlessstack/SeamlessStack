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
#if 0
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

