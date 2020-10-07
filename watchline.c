/* cscope.c
 *
 * History
 * 05/14/96 tony - created
 */

#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>

#include <att8130.h>

FILE	*log = NULL;

static	Device	dev1;
static	Device	dev2;
static	Device	*lastLogDev = NULL;

static int CheckIn(Device *dev, fd_set *inset)
{
	unsigned char	ch;

	if(FD_ISSET(dev->in, inset)) {
		read(dev->in, &ch, 1);
		
		if(lastLogDev != dev) {
			lastLogDev = dev;
			fprintf(log, "\n%s: ", dev->tag);
		}
		if(ch & 0x80) fprintf(log, "M-");
		fprintf(log, "%c", ch & 0x7f);
		fflush(log);
		return ch;
	}
	return -1;
}


static void watch()
{
	fd_set	inset, outset, exceptset;
	fd_set	*gotIn, *gotOut, *gotExcept;
	int	width = 16;
	int	checkChar;
	unsigned char	ch;
	long	foo = 0;
	struct timeval	timeout;

	width = ulimit(4, 0);
	fprintf(log, "Begin watching, width=%d....\n", width);
	while(1) {
		int	stat;
		FD_ZERO(&inset);
		FD_ZERO(&outset);
		FD_ZERO(&exceptset);
		FD_SET(dev1.in, &inset);
		FD_SET(dev2.in, &inset);
		FD_SET(dev1.out, &outset);
		FD_SET(dev2.out, &outset);
		if(foo % 100 == 0) fprintf(log, "call select\n");
		timeout.tv_sec = 0;
		timeout.tv_usec = 500;
		gotIn = &inset;
		gotOut = &outset;
		gotExcept = &exceptset;
		stat = select(width, &gotIn, &gotOut, &gotExcept, NULL);
		if(foo % 100 == 0) fprintf(log, "select -> %d\n", stat);
		foo++;

		checkChar = CheckIn(&dev1, gotIn);
		if(ch >= 0) {
			ch = checkChar;
			write(dev2.out, &ch, 1);
		}
		checkChar = CheckIn(&dev2, gotIn);
		if(ch >= 0) {
			ch = checkChar;
			write(dev1.out, &ch, 1);
		}
	}
}

static void usage(int argc, char *argv[])
{
	fprintf(stderr, "usage: %s logfile device1 device2\n", argv[0]);
}

void cleanup(int sig)
{
	fprintf(log, "Got signal %d\n", sig);
	closeDevice(&dev1);
	closeDevice(&dev2);
	fclose(log);
	exit(0);
}

int main(int argc, char *argv[])
{
	log = stdout;
	if(argc != 4) {
		usage(argc, argv);
		exit(1);
	}

	if(strcmp(argv[1], "-") == 0) {
		log = stdout;
	} else {
		log = fopen(argv[1], "w");
		if(log == NULL) {
			fprintf(stderr, "Cannot open log file '%s'\n", argv[1]);
			exit(1);
		}
	}

	fprintf(log, "Open %s\n", argv[2]);
	if(openDevice(argv[2], &dev1)) {
		exit(1);
	}
	dev1.tag = "L1";
	fprintf(log, "Open %s\n", argv[3]);
	if(openDevice(argv[3], &dev2)) {
		exit(1);
	}
	dev2.tag = "L2";

	fprintf(log, "Install signal handlers\n");
	/* signal(SIGHUP, cleanup); */
	signal(SIGINT, cleanup);
	signal(SIGQUIT, cleanup);
	signal(SIGILL, cleanup);
	signal(SIGTERM, cleanup);
	watch();
	return 0;
}
