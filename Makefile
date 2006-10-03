#
# Flake Main Makefile
#
include config.mak

VPATH=$(SRC_PATH)

CFLAGS=$(OPTFLAGS) -I. -I$(SRC_PATH) -I$(SRC_PATH)/libflake \
	-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -D_ISOC9X_SOURCE \
	-DHAVE_CONFIG_H

LDFLAGS+= -g

FLAKE_LIBDIRS = -L./libflake
FLAKE_LIBS = -lflake$(BUILDSUF)

all: lib progs utils
    
lib:
	$(MAKE) -C libflake all

progs:
	$(MAKE) -C flake all

utils:
	$(MAKE) -C util all

.PHONY: install

install: install-progs install-libs install-headers

install-progs: progs
	$(MAKE) -C flake install

install-libs:
	$(MAKE) -C libflake install-libs

install-headers:
	$(MAKE) -C libflake install-headers

uninstall: uninstall-progs uninstall-libs uninstall-headers

uninstall-progs:
	$(MAKE) -C flake uninstall-progs

uninstall-libs:
	$(MAKE) -C libflake uninstall-libs

uninstall-headers:
	$(MAKE) -C libflake uninstall-headers

dep:	depend

depend:
	$(MAKE) -C libflake depend
	$(MAKE) -C flake    depend
	$(MAKE) -C util     depend

clean:
	$(MAKE) -C libflake clean
	$(MAKE) -C flake    clean
	$(MAKE) -C util     clean
	rm -f *.o *.d *~

distclean: clean
	$(MAKE) -C libflake distclean
	$(MAKE) -C flake    distclean
	$(MAKE) -C util     distclean
	rm -f .depend config.*

# tar release (use 'make -k tar' on a checkouted tree)
FILE=flake-$(shell grep "\#define FLAKE_VERSION " libflake/flake.h | \
                    cut -d " " -f 3 )

tar: distclean
	rm -rf /tmp/$(FILE)
	cp -r . /tmp/$(FILE)
	(tar --exclude config.mak --exclude .svn -C /tmp -jcvf $(FILE).tar.bz2 $(FILE) )
	rm -rf /tmp/$(FILE)

config.mak:
	touch config.mak

.PHONY: lib

ifneq ($(wildcard .depend),)
include .depend
endif
