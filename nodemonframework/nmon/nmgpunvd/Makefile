TARGET=nmgpunvd

CONFIGFILE=$(ETCDIR)/nmon/$(TARGET).conf

IDIR += -I$(shell pwd)

CFLAGS=-O2 -pipe -g -Wall -DCONFIGFILE=\"$(CONFIGFILE)\" -DAPPNAME=\"$(TARGET)\" $(IDIR) -DNO_NVML_DEPENDENCY
LIBS=-ldl

.PHONY: all clean


all: $(TARGET).o
	${DIET} $(CC) $(STATIC) $^ ${LIBS} -o $(TARGET)

%.o: %.c
	${DIET} $(CC) $(CFLAGS) -c -o $@ $^

clean:
	$(RM) *.o $(TARGET)


install:
	$(INSTALL) -m 0755 -d $(DESTDIR)/$(BINDIR)
	$(INSTALL) -m 0755 -d $(DESTDIR)/$(ETCDIR)/nmon
	$(INSTALL) -m 0755 $(TARGET) $(DESTDIR)/$(BINDIR)/
	$(INSTALL) -m 0644 $(TARGET).conf $(DESTDIR)/$(ETCDIR)/nmon/

