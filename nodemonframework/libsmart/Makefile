LIBNAME=libsmart
CFLAGS=-O2 -g -pipe -Wall -fPIC $(IDIR)

SRC:=$(wildcard *.c)
OBJ:=$(patsubst %.c, %.o, $(SRC))

.PHONY: all clean install static dynamic

all: static dynamic

dynamic: $(OBJ)
	$(CC) -shared -o $(LIBNAME).so $^

static: $(OBJ)
	$(AR) cr $(LIBNAME).a $^

clean:
	$(RM) $(LIBNAME).so $(LIBNAME).a $(OBJ)

install:
	$(INSTALL) -m 0755 -d $(DESTDIR)/$(LIBDIR)
	$(INSTALL) -m 0755 -d $(DESTDIR)/$(INCDIR)
	$(INSTALL) -m 0755 $(LIBNAME).so $(DESTDIR)/$(LIBDIR)/
	$(INSTALL) -m 0644 $(LIBNAME).a  $(DESTDIR)/$(LIBDIR)/
	$(INSTALL) -m 0644 $(LIBNAME).h  $(DESTDIR)/$(INCDIR)/


