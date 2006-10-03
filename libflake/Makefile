#
# Flake Library Makefile
#
include ../config.mak

CFLAGS=$(OPTFLAGS) -I. -I.. -I$(SRC_PATH)/libflake \
	-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_ISOC9X_SOURCE \
	-DHAVE_CONFIG_H

OBJS= crc.o encode.o lpc.o md5.o optimize.o rice.o vbs.o \


HEADERS = flake.h

NAME=flake

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

install: install-libs install-headers

install-libs: $(LIB)
	install -d "$(libdir)"
	install -m 644 $(LIB) "$(libdir)"
	$(LIB_INSTALL_EXTRA_CMD)

install-headers:
	install -d "$(incdir)"
	install -m 644 $(addprefix "$(SRC_DIR)"/,$(HEADERS)) "$(incdir)"

uninstall: uninstall-libs uninstall-headers

uninstall-libs:
	-rm -f "$(libdir)/$(LIB)"

uninstall-headers:
	rm -f "$(addprefix $(incdir)/,$(HEADERS))"

../config.mak:
	touch ../config.mak

#
# include dependency files if they exist
#
ifneq ($(wildcard .depend),)
include .depend
endif
