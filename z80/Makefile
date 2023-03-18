# $Id: Makefile,v 1.1 2002/06/10 21:59:46 jkf Exp $

CFLAGS = ${KOS_CFLAGS} ${KOS_ALL_INCS} -DLSB_FIRST
#CFLAGS = ${KOS_CFLAGS} ${KOS_ALL_INCS}

all: z80.o

z80.o: z80.c
	${KOS_CC} $(CFLAGS) -c -o $@ $<

clean:
	/bin/rm -f z80.o

