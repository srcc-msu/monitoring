PWD:=$(shell pwd)
IDIR:="-I$(PWD)/libsmart -I$(PWD)/libsysnfo"
LDIR:="-L$(PWD)/libsmart -L$(PWD)/libsysnfo"

ifndef DIST
DIST := clustrx
endif

MACH := $(shell uname -m)

ifeq ($(MACH),ppc64)
DIET :=
endif

.PHONY: all clean install libsmart nmon sysnfo hybmond

all: libsmart nmon libsysnfo hybmond

libsmart:
	IDIR=$(IDIR) LDIR=$(LDIR) $(MAKE) -C $@

nmon: libsmart libsysnfo
	IDIR=$(IDIR) DIST=$(DIST) DIET=$(DIET) STATIC="$(STATIC)" LDIR=$(LDIR) EXCLUDE_MODULES="$(EXCLUDE_MODULES)" $(MAKE) -C $@

libsysnfo: libsmart
ifneq ($(MACH),ppc64)
	IDIR=$(IDIR) DIET=$(DIET) LDIR=$(LDIR) $(MAKE) -C $@
endif

hybmond: libsmart
	IDIR=$(IDIR) DIST=$(DIST) LDIR=$(LDIR) $(MAKE) -C $@ 


clean:
	$(MAKE) -C libsmart clean
	$(MAKE) -C nmon clean
ifneq ($(MACH),ppc64)
	$(MAKE) -C libsysnfo clean
endif
	$(MAKE) -C hybmond clean


install:
	$(MAKE) -C libsmart install
	$(MAKE) DIST=$(DIST) -C nmon install
ifneq ($(MACH),ppc64)
	$(MAKE) -C libsysnfo install
endif
	$(MAKE) -C hybmond install

