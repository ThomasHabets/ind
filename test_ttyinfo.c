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
        if (1) {
                for(;;) {
                        terminfo(0);
                        sleep(1);
                }
        }
}
