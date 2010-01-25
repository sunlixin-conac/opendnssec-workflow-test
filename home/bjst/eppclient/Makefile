DEBUG = -g
#OPTIMIZE = -O2
CFLAGS = -Wall -std=gnu99 $(DEBUG) $(OPTIMIZE)
LDFLAGS = $(DEBUG) $(OPTIMIZE)

TARGETS = eppclientd eppclient

all: $(TARGETS)
eppclientd: eppclientd.o config.o -lsqlite3 -lxml2
eppclient: eppclient.o config.o -lsqlite3
clean:
	rm -f $(TARGETS) *.o
