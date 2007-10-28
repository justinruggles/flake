#
# Flake Program Makefile
#
include ../config.mak

CFLAGS=$(OPTFLAGS) -I. -I.. -I$(SRC_PATH)/libflake \
	-I$(SRC_PATH)/libpcm_io \
	-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_ISOC9X_SOURCE \
	-DHAVE_CONFIG_H

LDFLAGS+= -g

DEP_LIBS=$(SRC_PATH)/libflake/$(LIBPREF)flake$(LIBSUF) $(SRC_PATH)/libpcm_io/$(LIBPREF)pcm_io$(LIBSUF)

PROGS_G=flake_g$(EXESUF)
PROGS=flake$(EXESUF)

OBJS = flake.o
SRCS = $(OBJS:.o=.c)
FLAKE_LIBDIRS = -L$(SRC_PATH)/libflake -L$(SRC_PATH)/libpcm_io
FLAKE_LIBS = -lflake$(BUILDSUF) -lpcm_io$(BUILDSUF)

all: $(PROGS_G) $(PROGS)

flake_g$(EXESUF): $(OBJS) $(DEP_LIBS)
	$(CC) $(FLAKE_LIBDIRS) $(LDFLAGS) -o $@ $(OBJS) $(FLAKE_LIBS) $(EXTRALIBS)
	cp -p flake_g$(EXESUF) flake$(EXESUF)
	$(STRIP) flake$(EXESUF)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

install: install-progs

install-progs: $(PROGS)
	install -d "$(bindir)"
	install -c $(INSTALLSTRIP) -m 755 $(PROGS) "$(bindir)"

uninstall: uninstall-progs

uninstall-progs:
	rm -f $(addprefix $(bindir)/, $(PROGS))

dep:	depend

depend: .depend

.depend: $(SRCS)
	$(CC) -MM $(CFLAGS) $^ 1>.depend

clean:
	rm -f *.o *.d *~ *.a *.lib *.def *.exp \
	   $(PROGS) $(PROGS_G)

distclean: clean
	rm -f .depend ../config.*

../config.mak:
	touch ../config.mak

ifneq ($(wildcard .depend),)
include .depend
endif
