# $Id$

DEBUG = -g
#OPTIMIZE = -O2
CFLAGS = -Wall -std=gnu99 $(DEBUG) $(OPTIMIZE)
LDFLAGS = $(DEBUG) $(OPTIMIZE)

TARGETS = eppclientd eppclient

all: $(TARGETS)
eppclientd: eppclientd.o config.o epp.o ezxml.o -lsqlite3 -lssl
eppclient: eppclient.o config.o
clean:
	rm -f $(TARGETS) *.o
