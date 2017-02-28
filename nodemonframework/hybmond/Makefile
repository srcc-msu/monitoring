TARGET=hybmond
PLUGIN_PREFIX=hm
IDIR += -I$(shell pwd)
LDIR += -L$(shell pwd)
CONFIGFILE=\"$(ETCDIR)/$(TARGET).conf\"
PIDFILE=\"$(RUNDIR)/$(TARGET).pid\"
PLUGIN_PREFIX_STR=\"$(PLUGIN_PREFIX)\"
PLUGINS_LOCATION=$(LIBDIR)/hybmond/
PLUGINS_LOCATION_STR=\"$(PLUGINS_LOCATION)\"

SRC:=main.c plugin_ctl.c
OBJ:=$(patsubst %c, %o, $(SRC))

LIBPL=lib$(PLUGIN_PREFIX)plugin.so
LIBPLSRC:=plugins_api.c plugin_err.c proto_err.c urlcode.c
LIBPLOBJ:=$(patsubst %c, %o, $(LIBPLSRC))

CFLAGS=-O2 -g -pipe -Wall -fPIC -DPLUGIN_PREFIX=$(PLUGIN_PREFIX_STR) -DPLUGINS_LOCATION=$(PLUGINS_LOCATION_STR) -DCONFIGFILE=$(CONFIGFILE) -DPIDFILE=$(PIDFILE)
LIBS=-l$(PLUGIN_PREFIX)plugin

.PHONY: all libpl plugins clean install

all: libpl daemon plugins $(TARGET).init

daemon: $(OBJ) libpl
	$(CC) -o $(TARGET) $(OBJ) -ldl $(LIBS) $(LDIR)

plugins: libpl
	IDIR="$(IDIR)" LDIR="$(LDIR)" LIBS="$(LIBS)" $(MAKE) -C plugins PLUGIN_PREFIX=$(PLUGIN_PREFIX)

libpl: $(LIBPLOBJ)
	$(CC) -shared $^ -o $(LIBPL)

$(TARGET).init: $(TARGET).init.$(DIST)
	sed -e "s,_BIN_PREFIX_,$(BINDIR),g;s,_LIB_PREFIX_,$(LIBDIR),g" < $< > $@

clean:
	$(RM) $(LIBPL) $(LIBPLOBJ)
	$(RM) -r $(OBJ) $(TARGET) $(PLUGINS_LOCATION) $(TARGET).init
	$(MAKE) -C plugins clean PLUGIN_PREFIX=$(PLUGIN_PREFIX)

install:
	$(INSTALL) -m 0755 -d $(DESTDIR)/$(BINDIR)
	$(INSTALL) -m 0755 -d $(DESTDIR)/$(LIBDIR)
	$(INSTALL) -m 0755 -d $(DESTDIR)/$(ETCDIR)
	$(INSTALL) -m 0755 -d $(DESTDIR)/$(INITDIR)
	$(INSTALL) -m 0755 -d $(DESTDIR)/$(LIBDIR)/$(TARGET)
	$(INSTALL) -m 0755 $(TARGET) $(DESTDIR)/$(BINDIR)/
	$(INSTALL) -m 0755 $(LIBPL)  $(DESTDIR)/$(LIBDIR)/
	$(INSTALL) -m 0644 $(TARGET).conf $(DESTDIR)/$(ETCDIR)/
	$(INSTALL) -m 0755 plugins/*/*.so $(DESTDIR)/$(LIBDIR)/$(TARGET)/
	$(INSTALL) -m 0755 $(TARGET).init $(DESTDIR)/$(INITDIR)/$(TARGET)


