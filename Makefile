OBJ=att8130d.o opendev.o
CC=acc
CDEBUGFLAGS=-g
INCLUDES=-I.
CFLAGS=$(CDEBUGFLAGS) $(INCLUDES)

live:	att8130d
	xterm -e att8130d `tty` /dev/ttyb -

test:	att8130d
	xterm -e att8130d log /dev/ttyb - &
	sleep 2
	tail -f log

att8130d:	$(OBJ)
	$(CC) -o $@ $(OBJ)

save:
	tar cvf /mnt/frito/usr/home/tony/att8130.tar Makefile PhoneDB *.h *.c doc
