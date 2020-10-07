/* cscope.c
 *
 * History
 * 05/14/96 tony - created
 */

#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#define	DEBUG	1

#include <att8130.h>


FILE	*log = NULL;

static	Device	PhonePort;
static	int	ringing = 0;		/* Which lines are ringing */
#define	CMD(s)	write(PhonePort.out, s, strlen(s));



/* Forward declarations */

static int	mainLoop();
static int	decode8B(int b1, int b2);
static int	DecodeLog(LogEntry *t, const unsigned char *buf, int len);
static void	PrintLog(const LogEntry *t, FILE *out);


#define	L_RING		0x4000
#define	L_SPKR		0x1000
#define	L_INUSE		0x0400
#define	L_FREE		0x0200
#define	L_PROBLEM	0x0100
#define	L_LIGHT		0x0008
#define	L_HEADSET	0x0004
#define	L_HOLD		0x0001

#define	KNOWNFLAGS	0x570d

static void ShowLineStatus(FILE *log, int line, int b2, int b3, int b4)
{
	unsigned long flags = b2 << 8 | b3;
	fprintf(log, "Status: line %d:", line);

	if(flags & L_RING) {
		char	tmp[20];
		sprintf(tmp, "AT+TCLT%d\r", line);
		CMD(tmp);
		ringing |= (1 << (line-1));
	} else {
		ringing &= ~(1 << (line-1));
	}

	if(flags & L_HEADSET) fprintf(log, " HeadsetUp");
	if(flags & L_RING) fprintf(log, " Ringing");
	if(flags & L_SPKR) fprintf(log, " OnSpeaker");
	if(flags & L_INUSE) fprintf(log, " InUse");
	if(flags & L_FREE) fprintf(log, " Free");
	if(flags & L_PROBLEM) fprintf(log, " Problem");
	if(flags & L_LIGHT) fprintf(log, " LightOn");
	if(flags & L_HOLD) fprintf(log, " OnHold");
	flags &= ~KNOWNFLAGS;
	fprintf(log, " %04x", flags);
	if(b4 != 0xff) {
		fprintf(log, " + <%02x>", b4);
	}
}

static void ShowData(const char *tag, const unsigned char *buf, int len)
{
	int	i;
	int	ch;
	int	b1,b2,b3,b4,b5;
	const unsigned char *end;

	end = buf + len;

	if(tag) fprintf(log, "%s %d bytes: ==>", tag, len);
	while(buf < end) {
		ch = *buf++;
		if(ch == '\0') {
			fprintf(log, "<NUL>");
			break;
		} else if(ch == '\r') {
			fprintf(log, "\\r\n\t");
		} else if(ch == '\n') {
			fprintf(log, "\\n\n\t");
		} else if(ch == '\t') {
			fprintf(log, "\\t");
		} else if(ch == 255) {
			fprintf(log, "<DEL>");
		} else if(ch < ' ') {
			fprintf(log, "<%02x>", ch);
		} else if(ch == 0x80 && *buf == 0x01) {	/* STATUS */
			b1 = *buf++;		/* eat the 01 */
			b1 = *buf++;
			b2 = *buf++;
			b3 = *buf++;
			b4 = *buf++;
			ShowLineStatus(log, b1, b2, b3, b4);
		} else if(ch == 0x8b) {
			b1 = *buf++;
			b2 = *buf++;
			if(decode8B(b1, b2) == 0) {
				fprintf(log, "<8b %02x %02x>", b1, b2);
				break;
			}
		} else if(ch == 0x88) {
			ch = *buf++;
			fprintf(log, "Transmit(%c)", ch);
		} else if(ch == 0xe1) {
			int		used;
			LogEntry	l;
			/* have to back up because we allready advanced */
			buf--;
			used = DecodeLog(&l, buf, end-buf);
			if(used > 0) {
				buf += used;
				PrintLog(&l, log);
			}
		} else if(ch == 0xe2) {
			char	pn[20], *p;
			fprintf(log, " DIAL=");
			p = pn;
			while(*buf != '\r') {
				*p++ = *buf;
				fprintf(log, "%c", *buf);
				buf++;
			}
			*p = '\0';
			if(1 || ringing) {
				char	cmd[100];
				sprintf(cmd, "grep %s PhoneDB", pn);
				system(cmd);
			}
		} else if(ch & 0x80) {
			fprintf(log, "<%02x>", ch);
		} else {
			fprintf(log, "%c", ch & 0x7f);
		}
	}
	if(tag) fprintf(log,"<==\n");
	fflush(log);
}


static int Command(const char *cmd)
{
	int	len = strlen(cmd);

	ShowData("Sending Command", (const unsigned char *)cmd, len);
	return write(PhonePort.out, cmd, len);
}

static unsigned char response[1000];		/* last response */
static int	responseLength = 0;

static int GetResponse(int showit)
{
	int	nread, tries = 0;

	responseLength = 0;
	while(tries < 2) {
		nread = read(PhonePort.in, &response[responseLength],
					sizeof(response)-responseLength);
		if(nread > 0) {
			responseLength += nread;
			tries = 0;
		} else {
			tries += 1;
#if 1
			usleep(100000);
#else
			sleep(1);
#endif
		}
	}
	if(responseLength > 0 && showit) {
		ShowData("Got", response, responseLength);
	}
	return responseLength;
}


/* Decode a Log  sequence (begins with e1) and prints it */
static int DecodeLog(LogEntry *t, const unsigned char *buf, int len)
{
	int	used = 0;

	t->line = 255;
	t->f1 = 0;
	t->f2 = 0;
	t->f3 = 0;
	if(buf[0] != 0xe1) return used;

	used++;
	if(used >= len) return used;
	t->hour = buf[used];

	used++;
	if(used >= len) return used;
	t->minute = buf[used];

	used++;
	if(used >= len) return used;
	t->second = buf[used];

	used++;
	if(used >= len) return used;
	t->endHour = buf[used];

	used++;
	if(used >= len) return used;
	t->endMinute = buf[used];

	used++;
	if(used >= len) return used;
	t->endSecond = buf[used];

	used++;
	if(used >= len) return used;
	t->month = buf[used];

	used++;
	if(used >= len) return used;
	t->day = buf[used];

	used++;
	if(used >= len) return used;
	t->year = buf[used];

	used++;
	if(used >= len) return used;
	t->line = buf[used];

	used++;
	if(used >= len) return used;
	if(buf[used] == 0xff) return used;
	if(buf[used] == '\r') return used;
	t->f1 = buf[used];

	used++;
	if(used >= len) return used;
	if(buf[used] == '\r') return used;
	t->f2 = buf[used];

	used++;
	if(used >= len) return used;
	if(buf[used] == '\r') return used;
	t->f3 = buf[used];

	return used;
}

static void PrintLog(const LogEntry *t, FILE *out)
{
	fprintf(out, "    L%d: ", t->line);
	if(t->f1 & 0x20) {
		fprintf(out, "Out");
	} else {
		fprintf(out, " In");
	}
	if(t->f1 & 0x01) {
		fprintf(out, " A");
	} else {
		fprintf(out, "  ");
	}
	fprintf(out, " %2d:%02d:%02d", t->hour, t->minute, t->second);
	fprintf(out, " %2d:%02d:%02d", t->endHour, t->endMinute, t->endSecond);
	fprintf(out, " %2d/%02d/%02d", t->month, t->day, t->year);

	fprintf(out, " <%02x %02x %02x>", t->f1, t->f2, t->f3);
}


static void GetCallLog()
{
	unsigned char	*r;
	LogEntry	l;


#if SAMPLE
+TCLF		download call log
	4669766	01, 3:51pm 05/20 1R
	L2: (e1)(0f)2(12)(0f)2-(05)(14)`(01)(02)(00)\r(e2)5164669766\rOK\r
		 hh mm ss hh mm ss mon day yy ring ?? line? 5164669766
+TCL 		next until returns ERROR

#endif
	CMD("AT+TCLF\r");
	while(1) {
		GetResponse(0);
		for(r = response; r < response + responseLength; r++) {
			int	left = response+responseLength-r;
			if(*r == 0xe1) {
				int	used;
				used = DecodeLog(&l, r, left);
				if(used > 0) {
					r += used;
					PrintLog(&l, log);
				}
			} else if(*r == 0xe2) {
				r++;
				fprintf(log, " # = ");
				while(*r != '\r') {
					fprintf(log, "%c", *r);
					r++;
				}
			} else if(*r == 'O' && *(r+1) == 'K') {
				fprintf(log, "\n");
				CMD("AT+TCL\r");
				r++;
			} else if(strncmp(r, "ERROR", 5) == 0) {
				fprintf(log, "\n");
				goto done;
			} else if(*r == '\r') {
				;
			} else {
				fprintf(log, "<%02x>", *r);
			}
		}
	}
done:
	responseLength = 0;
}



static void usage(int argc, char *argv[])
{
	fprintf(stderr, "usage: %s logfile PhoneDevice\n", argv[0]);
}

void cleanup(int sig)
{
	fprintf(log, "Got signal %d\n", sig);
	closeDevice(&PhonePort);
	fclose(log);
	exit(0);
}

void sigint(int sig)
{
#if GET_BREAKS
	if(sig == SIGINT) {
		fprintf(log, "<BREAK>\n");
	}
	printf("To Kill my use\n\nkill %d\n", getpid());
	return;
#else
	exit(0);
#endif
}



int main(int argc, char *argv[])
{

	log = stdout;
	if(argc < 3) {
		usage(argc, argv);
		exit(1);
	}

	if(strcmp(argv[1], "-") == 0) {
		log = stdout;
	} else {
		log = fopen(argv[1], "a");
		if(log == NULL) {
			fprintf(stderr, "Cannot open log file '%s'\n", argv[1]);
			exit(1);
		}
		fprintf(log, "\n\n============= New session =============\n");
	}

	fprintf(log, "Open %s\n", argv[2]);
	if(openDevice(argv[2], &PhonePort)) {
		exit(1);
	}
	PhonePort.tag = "L1";
	SetSpeed(&PhonePort, 1200);
	NoBlock(&PhonePort);

	fprintf(log, "Install signal handlers\n");
	/* signal(SIGHUP, cleanup); */
	signal(SIGINT, sigint);
	signal(SIGQUIT, cleanup);
	signal(SIGILL, cleanup);
	signal(SIGTERM, cleanup);

	sleep(1);
	Command("ATI4\r");
	GetResponse(1);
	Command("AT+TVL=2\r");
	GetResponse(1);
	Command("AT+TQA\r");
	GetResponse(1);

	mainLoop();
	return 0;
}

static int mainLoop()
{
	fd_set	infds, outfds, exceptfds;
	int	width = 16;
	int	stat;
	struct timeval	timeout;
#if DEBUG
	int	loopCount = 0;
#endif
	char	buf[500];		/* keyboard command input buffer */

	width = ulimit(4, 0);
	fprintf(log, "Begin watching, width=%d....\n", width);

	responseLength = 0;
	while(1) {
		char c8b[10];
		char c88[10];
		int	nread;
		int	stat;

#if DEBUG
		loopCount++;
#endif

		c8b[0] = 0x8b;
		c88[0] = 0x88;

		FD_ZERO(&infds);
		FD_ZERO(&outfds);
		FD_ZERO(&exceptfds);

		/* Look for input on device */
		FD_SET(PhonePort.in, &infds);
		FD_SET(fileno(stdin), &infds);	/* and control term */
#if 0
		FD_SET(PhonePort.out, &outfds);
#endif
		FD_SET(PhonePort.in, &exceptfds);
		timeout.tv_sec = 1;
		timeout.tv_usec = 200;
		stat = select(width, &infds, NULL, &exceptfds, &timeout);
#if DEBUG
		if(loopCount % 1000 == 0) fprintf(log, "select -> %d\n", stat);
#endif
		if(stat < 0) {
			perror("select");
			continue;
		}
		if(stat == 0) {
#if DEBUG
			if(loopCount % 100 == 0) {
				fprintf(log, "Timeout on select\n");
			}
#endif
		}

#if 1
		if(FD_ISSET(PhonePort.in, &exceptfds)) {
			fprintf(log, "Exception on device.\n");
		}
#endif
		if(FD_ISSET(PhonePort.in, &infds)) {
			/* printf("device ready\n"); */
			nread = read(PhonePort.in, &response[responseLength],
					sizeof(response)-responseLength);
			if(nread > 0) {
				/* printf("read %d bytes\n", nread); */
				responseLength += nread;
				continue;
			}
		} else {
			/* printf("device not ready\n"); */
		}

		if(responseLength > 0) {
			ShowData("Got", response, responseLength);
			responseLength = 0;
		}

		/* look for keyboard input */
		if(FD_ISSET(fileno(stdin), &infds)) {
			fgets((char *)buf, sizeof(buf), stdin);
			nread = strlen(buf);
			if(nread > 1) {
				if(buf[0] == '#') {
					buf[nread-1] = '\0';
					fprintf(log, "==== %s ====\n", &buf[1]);
					fflush(log);
				} else if(buf[0] == 'q') {
					break;
				} else if(buf[0] == 'l') {
					GetCallLog();
				} else if(buf[0] == 's') {
					Command("AT+TLS1?+TLS2?\r");
				} else if(buf[0] == 'D') {
					c8b[1] = 0x06;
					c8b[2] = buf[1] - '0';
					c8b[3] = '\0';
					Command(c8b);
				} else if(buf[0] == 'P') {
					c88[1] = buf[1];
					c88[2] = '\0';
					Command(c88);
				} else {
					fprintf(log, "Sending: %s", buf);
					fflush(log);
					if(buf[nread-1] == '\n') buf[nread-1] = '\r';
					/* ShowData("Typed", buf, nread);*/
					write(PhonePort.out, buf, strlen(buf));
				}
			}
		}
	}
	return 0;
}


#if SAMPLE

+TQA
+TQTA
+TLS2?		status line 2
	L2: (80)(01)(01)(02)(08)(ff)\r(80)(01)(02)(02)(00)(ff)\rOK\r
        L2: (80)(01)(01)(02)(08)(ff)\r(80)(01)(02)(02)(00)(ff)\rOK\r
	L2: (80)(01)(01)(02)(08)(ff)\r(80)(01)(02)(02)(00)(ff)\rOK\r


L1: AT+TTM=16:24:03\r\r
L2: (82)\rOK\r

+TD0=0
+TLO1=0
#endif



/* This decodes a response sequence initiated with an 0x8b */

static struct {
	int	c1;
	int	c2;
	char	*key;
} KeyCodes[] = {
	{ 0x02, 0x2a, "*" },
	{ 0x03, 0x23, "#" },
	{ 0x05, 0x46, "FLASH" },
	{ 0x07, 0x20, "SPACE" },
	{ 0x08, 0x00, "AUTODIAL" },
	{ 0x0c, 0x00, "SPKR" },
	{ 0x0f, 0x00, "MUTE" },
	{ 0x10, 0x00, "HOLD" },
	{ 0x12, 0x00, "CONF" },
	{ 0x13, 0x00, "LINE 1" },
	{ 0x14, 0x01, "LINE 2" },
	{ 0x18, 0x91, "CLOCK" },
	{ 0x18, 0x9d, "INLOG" },
	{ 0x18, 0xa0, "REDIAL+" },
	{ 0x18, 0xa2, "TIMER" },
	{ 0x18, 0xbd, "DIRECTORY" },
	{ 0x18, 0xc1, "REMOVE" },
	{ 0x18, 0xc5, "REMOVE ALL" },
	{ 0x1b, 0x00, "PROGRAM" },
	{ 0x1d, 0x00, "DISPLAY" },
	{ 0x1e, 0x00, "VOLUME DOWN" },
	{ 0x1e, 0x01, "VOLUME UP" },
	{ 0x23, 0xc1, "FEATURE" },
	{ 0x23, 0xc2, "SELECT" },
	{ 0x27, 0x00, "LOWER" }
};


static int decode8B(int b1, int b2)
{
	int	retcode = b1;
	int	i;
	char	tmp[50];

	switch(b1) {
	case 0x01:
		if(isprint(b2)) {
			fprintf(log, "KeyPad(%c)", b2);
		} else {
			fprintf(log, "KeyPress(0x%02x)", b2);
		}
		break;
	case 0x06:
		fprintf(log, "DoTouchTone?(%d)", b2);
		break;
	case 0x08:
		fprintf(log, "Telco Line %d Down", b2+1);
		break;
	case 0x09:
		fprintf(log, "Telco Line %d Up", b2+1);
		break;
	case 0x0a:
		fprintf(log, "RING Line %d", b2+1);
		sprintf(tmp, "AT+TCLT%d\r", b2+1);
		CMD(tmp);
		break;
	case 0x0d:
		fprintf(log, "ReceiverUp(0x%02x)", b2);
		break;
	case 0x0e:
		fprintf(log, "ReceiverDown(0x%02x)", b2);
		break;
#if 0
	case 0x18:
		if(b2 == 0x91) {
			fprintf(log, "CLOCK");
		} else if(b2 == 0x9d) {
			fprintf(log, "INLOG");
		} else {
			fprintf(log, "<8b %02x %02x>", b1, b2);
		}
		break;
#endif

	case 0x1c:
		fprintf(log, "SpeedDial(%d)", b2);
		break;
#if 0
	case 0x23:
		if(b2 == 0xc1) {
			fprintf(log, "Feature");
		} else if(b2 == 0xc2) {
			fprintf(log, "Select");
		} else {
			fprintf(log, "<8b %02x %02x>", b1, b2);
		}
#endif

	default:
		retcode = 0;		/* indicates a failure to decode */
		for(i = 0; i < sizeof(KeyCodes)/sizeof(KeyCodes[0]); i++) {
			if(KeyCodes[i].c1 != b1) continue;
			if(KeyCodes[i].c2 != b2) continue;
			fprintf(log, "Key<%s>", KeyCodes[i].key);
			retcode = 1;		/* success */
		}
	}
	return retcode;
}
