# $Id$

DEBUG = -g -DDEBUG
#OPTIMIZE = -O3
CFLAGS = -Wall -std=gnu99 $(DEBUG) $(OPTIMIZE)
LDFLAGS = $(DEBUG) $(OPTIMIZE)

quicksorter: quicksorter.o encode.o

clean:
	rm -f $(TARGETS) *.o
