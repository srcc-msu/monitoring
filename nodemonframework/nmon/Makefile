OBJ:=nm_cpu.o nm_mem.o nm_ifaces.o nm_hwstat.o try_oom_adj.o

IDIR += -I$(shell pwd)
TARGET=$(basename $@)
PIDFILE=$(RUNDIR)/$(TARGET).pid
CONFIGFILE=$(ETCDIR)/$(TARGET).conf

CFLAGS=-O2 -g -pipe -Wall -std=c99 $(IDIR)
DEFFLAGS=-DAPPNAME=\"$(TARGET)\" -DCONFIGFILE=\"$(CONFIGFILE)\" -DPIDFILE=\"$(PIDFILE)\"


SUBMODS_LIST=nmperfd nmfsd nmsmartd nmipmid nmiod nmibd nmhopsad nmgpunvd
SUBMODS=$(filter-out $(EXCLUDE_MODULES),$(SUBMODS_LIST))
KNMON=

DEFMCE:=
MACH := $(shell uname -m)

ifneq ($(MACH),ppc64)
OBJ += nm_dietquirk.o
SUBMODS+= nmsensd
ifeq ($(DIST),clustrx)
KNMON+= knmonctld knmonctld.init
endif
DEFFLAGS+= -DSENSMOD
DEFMCE += -DX86
endif

ifeq ($(MACH),x86_64)
DEFMCE += -DMCE
endif

SUBMODSCLEAN= $(patsubst %,%clean,$(SUBMODS))
SUBMODSINSTALL= $(patsubst %,%install,$(SUBMODS))

.PHONY: all core clean $(SUBMODS) $(SUBMODS_CLEAN) $(SUBMODS_INSTALL)


all: nmond nmond.init $(SUBMODS) $(KNMON)

nmond: nmond.o nm_control.o $(OBJ)
	${DIET} $(CC) $(STATIC) -o $@ $^

knmonctld: knmonctld.o knm_control.o nm_hwstat.o nm_dietquirk.o try_oom_adj.o
	${DIET} $(CC) -o $@ $^

nmond.o: main.c
	${DIET} $(CC) -c $(CFLAGS) -D_USERLAND $(DEFFLAGS) -o $@ $^

knmonctld.o: main.c
	${DIET} $(CC) -c $(CFLAGS) $(DEFFLAGS) -o $@ $^

nm_control.o: nm_control.c
	${DIET} $(CC) -c $(CFLAGS) -D_USERLAND $(DEFFLAGS) -o $@ $^

knm_control.o: nm_control.c
	${DIET} $(CC) -c $(CFLAGS) $(DEFFLAGS) -o $@ $^

nmond.init: nmond.init.$(DIST)
	sed -e "s,_BIN_PREFIX_,$(BINDIR),g;s,_LIB_PREFIX_,$(LIBDIR),g" < $< > $@

knmonctld.init: knmonctld.init.$(DIST)
	sed -e "s,_BIN_PREFIX_,$(BINDIR),g;s,_LIB_PREFIX_,$(LIBDIR),g" < $< > $@

$(SUBMODS): nm_modshm.o
	IDIR="$(IDIR)" DIET="$(DIET)" STATIC="$(STATIC)" LDIR="$(LDIR)" $(MAKE) -C $@

%.o: %.c
	${DIET} $(CC) $(CFLAGS) $(DEFMCE) -c -o $@ $^

clean: $(SUBMODSCLEAN)
	$(RM) *.o nmond knmonctld nmond.init knmonctld.init

$(SUBMODSCLEAN):
	$(MAKE) -C $(subst clean,,$@) clean

install: $(SUBMODSINSTALL)
	$(INSTALL) -m 0755 -d $(DESTDIR)/$(BINDIR)
	$(INSTALL) -m 0755 -d $(DESTDIR)/$(INITDIR)
	$(INSTALL) -m 0755 -d $(DESTDIR)/$(ETCDIR)
ifeq ($(DIST),clustrx)
	$(INSTALL) -m 0755 -d $(DESTDIR)/$(ETCDIR)/sysconfig
ifneq ($(MACH),ppc64)
	$(INSTALL) -m 0755 knmonctld $(DESTDIR)/$(BINDIR)/
	$(INSTALL) -m 0644 knmonctld.conf $(DESTDIR)/$(ETCDIR)/
	$(INSTALL) -m 0644 knmonctld.sysconfig $(DESTDIR)/$(ETCDIR)/sysconfig/knmonctld
	$(INSTALL) -m 0755 knmonctld.init $(DESTDIR)/$(INITDIR)/knmonctld
endif
	$(INSTALL) -m 0644 nmond.sysconfig $(DESTDIR)/$(ETCDIR)/sysconfig/nmond
endif
	$(INSTALL) -m 0755 nmond $(DESTDIR)/$(BINDIR)/
	$(INSTALL) -m 0644 nmond.conf $(DESTDIR)/$(ETCDIR)/
	$(INSTALL) -m 0755 nmond.init $(DESTDIR)/$(INITDIR)/nmond

$(SUBMODSINSTALL):
	$(MAKE) -C $(subst install,,$@) install


