       #include <stdio.h>
       #include <unistd.h>

int main()
{
	//setvbuf(stdout, (char *)NULL, _IOLBF, 0);
	//setvbuf(stderr, (char *)NULL, _IOLBF, 0);

	fprintf(stdout, "0.1 stdout\n");
	sleep(1);
	fprintf(stderr, "0.2 stderr\n");
	sleep(1);
	fprintf(stdout, "0.3 stdout\n");
	fflush(stdout);
	fflush(stderr);
	sleep(3);
	fprintf(stderr, "1.1 stdout\n");
	fprintf(stderr, "1.2 stderr\n");
	fprintf(stderr, "1.3 stdout\n");
}
