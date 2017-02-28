PLUGINS:=$(shell ls | grep -v Makefile)

.PHONY: all clean

all:
	for p in $(PLUGINS); do IDIR="$(IDIR)" LDIR="$(LDIR)" LIBS="$(LIBS)" $(MAKE) -C $$p PLUGIN_PREFIX=$(PLUGIN_PREFIX); done

clean:
	for p in $(PLUGINS); do $(MAKE) -C $$p clean PLUGIN_PREFIX=$(PLUGIN_PREFIX); done
