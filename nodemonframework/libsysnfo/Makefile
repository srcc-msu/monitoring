ALIB=libsysnfo.a
SOLIB=libsysnfo.so
TINYLIB=libtinysysnfo.a
SRC=sysnfo.c
OBJ=sysnfo.o
CFLAGS=-O2 -pipe -Wall -fPIC $(IDIR)

MACH := $(shell uname -m)


.PHONY=all clean install

all: $(ALIB) $(SOLIB) $(TINYLIB)

$(ALIB): $(OBJ)
	$(AR) cr $@ $^

$(SOLIB): $(OBJ)
	$(CC) -shared -o $@ $^ -lsmart $(LDIR)

$(TINYLIB):
	${DIET} $(CC) -c $(CFLAGS) -DTINYSYSNFO -o tinysysnfo.o $(SRC)
	${DIET} $(AR) cr $@ tinysysnfo.o

clean:
	$(RM) *.o $(ALIB) $(SOLIB) $(TINYLIB)

install:
	$(INSTALL) -m 755 -d $(DESTDIR)/$(LIBDIR)
	$(INSTALL) -m 755 -d $(DESTDIR)/$(INCDIR)
	$(INSTALL) -m 0755 $(SOLIB) $(DESTDIR)/$(LIBDIR)/
	$(INSTALL) -m 0644 $(ALIB)  $(DESTDIR)/$(LIBDIR)/
	$(INSTALL) -m 0644 sysnfo.h  $(DESTDIR)/$(INCDIR)/

