#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <termios.h>


static void
terminfo(int fd)
{
	struct winsize w;

	printf("fd: %d\n", fd);
	if (!isatty(fd)) {
		printf("\tNot a tty!\n");
		return;
	}
	if (-1 == ioctl(fd, TIOCGWINSZ, &w)) {
		printf("\tioctl(TIOCGWINSZ) fail: %s\n", strerror(errno));
		return;
	}
	printf("\tSize: %dx%d\n", w.ws_row, w.ws_col);
}

int main()
{
	if (0) {
		setvbuf(stdout, (char *)NULL, _IOLBF, 0);
		setvbuf(stderr, (char *)NULL, _IOLBF, 0);
	}
	terminfo(0);
	terminfo(1);
	terminfo(2);

	fprintf(stdout, "0.1 stdout\n");
	sleep(1);
	fprintf(stderr, "0.2 stderr\n");
	sleep(1);
	fprintf(stdout, "0.3 stdout\n");
	fflush(stdout);
	fflush(stderr);

	sleep(3);

	fprintf(stdout, "1.1 stdout\n");
	fprintf(stderr, "1.2 stderr\n");
	fprintf(stdout, "1.3 stdout\n");

	return 0;
}
