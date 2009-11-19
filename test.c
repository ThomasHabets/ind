#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>


static void
terminfo(int fd)
{
	struct winsize w;
	char *tty;

	printf("fd: %d\n", fd);
	if (!isatty(fd)) {
		printf("\tNot a tty!\n");
		return;
	}
	if (-1 == ioctl(fd, TIOCGWINSZ, &w)) {
		printf("\tioctl(TIOCGWINSZ) fail: %s\n", strerror(errno));
		return;
	}
	tty = ttyname(fd);
	if (tty) {
		printf("\tttyname(): %s\n", tty);
	}
	printf("\tSize: %dx%d\n", w.ws_row, w.ws_col);
}

int main()
{
	if (0) {
		setvbuf(stdout, (char *)NULL, _IOLBF, 0);
		setvbuf(stderr, (char *)NULL, _IOLBF, 0);
	}
        if (1) {
                printf("ctermid(): %s\n", ctermid(0));
        }
	if (1) {
		terminfo(0);
		terminfo(1);
		terminfo(2);
	}

	if (1) {
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
		sleep(1);
	}

	/* \r test */
	if (1) {
		fprintf(stdout, "Progress: wait...");
		fflush(stdout);
		sleep(1);
		fprintf(stdout, "\rProgress: done   \n");
	}

	/* stdin test */
	if (1) {
		int nana;
		printf("Give me an integer: ");
		fflush(stdout);
		scanf("%d", &nana);
		printf("Value: %d\n", nana);
	}
	if (0) {
		char buf[1024];
		int n;
		printf("reading from stdin...\n"); fflush(stdout);
		n = read(0, buf, sizeof(buf));
		printf("done (%d)...\n", n); fflush(stdout);
	}
	if (0) {
		printf("Closing stdin...\n"); fflush(stdout);
		close(0);
		printf("done...\n"); fflush(stdout);
	}
	return 0;
}
