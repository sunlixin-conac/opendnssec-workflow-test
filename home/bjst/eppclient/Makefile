DEBUG = -g
#OPTIMIZE = -O2
CFLAGS = -Wall $(DEBUG) $(OPTIMIZE)
LDFLAGS = $(DEBUG) $(OPTIMIZE)

TARGETS = eppclientd eppclient

all: $(TARGETS)
eppclientd: eppclientd.o -lsqlite3 -lxml2
eppclient: eppclient.o -lsqlite3
clean:
	rm -f $(TARGETS) *.o
