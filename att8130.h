/* cscope.h
 *
 * History
 * 05/14/96 tony - created
 */


#define	OLD_TERMIO	0
#define GET_BREAKS	1

#if  OLD_TERMIO
#include	<sys/ioctl.h>
#else
#include	<termios.h>
#endif

typedef struct {
	char		*name;
	char		*tag;
	int		in;
	int		out;
#if OLD_TERMIO
	struct sgttyb	origState;
#else
	struct termios	origState;
#endif
} Device;


typedef struct {
	int	line;
	unsigned char	f1;
	unsigned char	f2;
	unsigned char	f3;
	short	hour;
	short	minute;
	short	second;
	short	endHour;
	short	endMinute;
	short	endSecond;
	short	month;
	short	day;
	short	year;
} LogEntry;

extern int openDevice(const char *, Device *);
extern int closeDevice(Device *);
extern void NoBlock(Device *);
extern void SetSpeed(Device *, int);


extern	FILE	*log;		/* trace log file */

