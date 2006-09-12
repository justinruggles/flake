#
# Flake Program Makefile
#
include ../config.mak

VPATH=$(SRC_PATH)

CFLAGS=$(OPTFLAGS) -I. -I.. -I$(SRC_PATH)/libflake \
	-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_ISOC9X_SOURCE

LDFLAGS+= -g

PROGS_G=flake_g$(EXESUF)
PROGS=flake$(EXESUF)

OBJS = flake.o wav.o
SRCS = $(OBJS:.o=.c)
FLAKE_LIBDIRS = -L../libflake
FLAKE_LIBS = -lflake$(BUILDSUF)

all: $(PROGS_G) $(PROGS)

flake_g$(EXESUF): flake.o wav.o libflake/libflake.a
	$(CC) $(FLAKE_LIBDIRS) $(LDFLAGS) -o $@ flake.o wav.o $(FLAKE_LIBS) $(EXTRALIBS)
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
	rm -f *.o *~ \
	   $(PROGS) $(PROGS_G)

distclean: clean
	rm -f .depend ../config.*

../config.mak:
	touch ../config.mak

ifneq ($(wildcard .depend),)
include .depend
endif
