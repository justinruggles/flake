#
# Flake libpcm_io Makefile
#
include ../config.mak

CFLAGS=$(OPTFLAGS) -I. -I.. \
	-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_ISOC9X_SOURCE \
	-DHAVE_CONFIG_H

OBJS= byteio.o convert.o pcm_io.o raw.o wav.o \


HEADERS = pcm_io.h

NAME = pcm_io

SRC_DIR = $(SRC_PATH)/lib$(NAME)
VPATH = $(SRC_DIR)

SRCS := $(OBJS:.o=.c)

all: $(EXTRADEPS) $(LIB)

$(LIB): $(OBJS)
	rm -f $@
	$(AR) rc $@ $^ $(EXTRAOBJS)
	$(RANLIB) $@

%.o: %.c
	$(CC) $(CFLAGS) $(LIBOBJFLAGS) -c -o $@ $<

depend: $(SRCS)
	$(CC) -MM $(CFLAGS) $^ 1>.depend

dep:	depend

clean:
	rm -f *.o *.d *~ *.a *.lib *.def *.exp

distclean: clean
	rm -f .depend

../config.mak:
	touch ../config.mak

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
