# $Id$

DEBUG = -g -DDEBUG
#OPTIMIZE = -O2
CFLAGS = -Wall -Wextra -funsigned-char -std=gnu99 $(DEBUG) $(OPTIMIZE) -I/usr/include/libxml2
LDFLAGS = $(DEBUG) $(OPTIMIZE)

TARGETS = eppclientd eppclient

all: $(TARGETS)
eppclientd: eppclientd.o config.o epp.o -lsqlite3 -lcurl -lxml2 -lldns 
eppclient: eppclient.o config.o -lxml2
clean:
	rm -f $(TARGETS) *.o
