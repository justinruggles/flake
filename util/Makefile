#
# Flake Util Makefile
#
include ../config.mak

CFLAGS=$(OPTFLAGS) -I. -I.. -I$(SRC_PATH)/flake -I$(SRC_PATH)/libflake \
	-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_ISOC9X_SOURCE \
	-DHAVE_CONFIG_H

LDFLAGS+= -g -L$(SRC_PATH)/libflake

PROGS=wavinfo$(EXESUF)

OBJS = wavinfo.o ../flake/wav.o
SRCS = $(OBJS:.o=.c)

all: $(PROGS)

wavinfo$(EXESUF): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(EXTRALIBS)

api_example$(EXESUF): api_example.o
	$(CC) $(LDFLAGS) -o $@ api_example.o -lflake $(EXTRALIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

install:

dep:	depend

depend: .depend

.depend: $(SRCS)
	$(CC) -MM $(CFLAGS) $^ 1>.depend

clean:
	rm -f *.o *~ \
	*.d *.a *.lib *.def *.exp \
	$(PROGS) \
	api_example

distclean: clean
	rm -f .depend ../config.*

../config.mak:
	touch ../config.mak

ifneq ($(wildcard .depend),)
include .depend
endif
