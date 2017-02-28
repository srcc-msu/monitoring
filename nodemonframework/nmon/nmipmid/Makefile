TARGET=nmipmid

IDIR += -I$(shell pwd)
CFLAGS=-O2 -g -pipe -Wall $(IDIR)

IDFDIR=$(SHDIR)/nmipmid/
DEFFLAGS=-DIDFDIR=\"$(IDFDIR)\" -DAPPNAME=\"nmipmid\"

SRC:=$(shell ls *.c)
OBJ:=$(patsubst %c, %o, $(SRC))
#LIBS=-lm -lsysnfo -lsmart
LIBS=-ltinysysnfo

.PHONY: idf clean install

all: idf $(TARGET)

$(TARGET): $(OBJ)
	${DIET} $(CC) $(STATIC) $(CFLAGS) -o $(TARGET) $^ ../nm_modshm.o $(LIBS) $(LDIR)

%.o: %.c
	${DIET} $(CC) -c $(CFLAGS) $(DEFFLAGS) -c -o $@ $^

idf:
	IDIR="$(IDIR)" $(MAKE) -C $@

clean:
	$(MAKE) -C idf clean
	$(RM) $(OBJ) $(TARGET)

install:
	$(MAKE) -C idf install
	$(INSTALL) -m 0755 -d $(DESTDIR)/$(BINDIR)
	$(INSTALL) -m 0755 nmipmid $(DESTDIR)/$(BINDIR)/



