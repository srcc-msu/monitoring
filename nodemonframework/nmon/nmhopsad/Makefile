TARGET=nmhopsad

CONFIGFILE=$(ETCDIR)/nmon/$(TARGET).conf

CFLAGS=-O2 -pipe -g -Wall -std=c99 -DCONFIGFILE=\"$(CONFIGFILE)\" -DAPPNAME=\"$(TARGET)\" $(IDIR)

.PHONY: all clean


all: $(TARGET).o
	${DIET} $(CC) $(STATIC) -o $(TARGET) $^ ../nm_modshm.o

%.o: %.c
	${DIET} $(CC) $(CFLAGS) -c -o $@ $^

clean:
	$(RM) *.o $(TARGET)


install:
	$(INSTALL) -m 0755 -d $(DESTDIR)/$(BINDIR)
	$(INSTALL) -m 0755 -d $(DESTDIR)/$(ETCDIR)/nmon
	$(INSTALL) -m 0755 $(TARGET) $(DESTDIR)/$(BINDIR)/
	$(INSTALL) -m 0644 $(TARGET).conf $(DESTDIR)/$(ETCDIR)/nmon/

