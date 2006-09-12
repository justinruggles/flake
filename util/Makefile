#
# Flake Util Makefile
#
include ../config.mak

VPATH=$(SRC_PATH)

CFLAGS=$(OPTFLAGS) -I. -I.. -I$(SRC_PATH)/flake \
       -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_ISOC9X_SOURCE

LDFLAGS+= -g

PROGS=wavinfo$(EXESUF)

OBJS = wavinfo.o ../flake/wav.o
SRCS = $(OBJS:.o=.c)

all: $(PROGS)

wavinfo$(EXESUF): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(EXTRALIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

install:

dep:	depend

depend: .depend

.depend: $(SRCS)
	$(CC) -MM $(CFLAGS) $^ 1>.depend

clean:
	rm -f *.o *~ \
	   $(PROGS)

distclean: clean
	rm -f .depend ../config.*

../config.mak:
	touch ../config.mak

ifneq ($(wildcard .depend),)
include .depend
endif
