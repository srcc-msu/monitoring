TARGET=nmibd

CONFIGFILE=$(ETCDIR)/nmon/$(TARGET).conf

CFLAGS=-O2 -pipe -g -Wall -std=c99 -DCONFIGFILE=\"$(CONFIGFILE)\" -DAPPNAME=\"$(TARGET)\" $(IDIR)
LIBS=-lnmib


.PHONY: all clean


all: $(TARGET).o
	${DIET} $(CC) $(STATIC) $< ../nm_modshm.o $(LIBS) -o $(TARGET)

$(TARGET).o: $(TARGET).c
	${DIET} $(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) *.o $(TARGET)


install:
	$(INSTALL) -m 0755 -d $(DESTDIR)/$(BINDIR)
	$(INSTALL) -m 0755 -d $(DESTDIR)/$(ETCDIR)/nmon
	$(INSTALL) -m 0755 $(TARGET) $(DESTDIR)/$(BINDIR)/
	$(INSTALL) -m 0644 $(TARGET).conf $(DESTDIR)/$(ETCDIR)/nmon/
