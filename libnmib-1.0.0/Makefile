SHELL = /bin/sh
CC ?= gcc
INSTALL ?= install
CFLAGS ?= -O2 -pipe -Wall -g
CFLAGS += -fPIC -DIGNORE_TEST_XMTWAIT

ifdef OLD_IBMAD_COMPAT
CFLAGS += -DOLD_IBMAD_COMPAT
endif

VERSION ?= 0.0.0
NAME := nmib
LIBNAME := lib$(NAME).so
SONAME := $(LIBNAME).$(shell echo $(VERSION)|sed -e 's/\..*//')
TARGET := $(SONAME).$(shell echo $(VERSION)|sed -e 's/^[0-9]*\.//')

SRC:=$(wildcard *.c)
OBJ:=$(patsubst %.c, %.o, $(SRC))

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) -shared -Wl,-soname,$(SONAME) $< -libmad -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJ) $(LIBNAME)*

install:
	$(INSTALL) -d $(DESTDIR)$(LIBDIR)
	$(INSTALL) -d $(DESTDIR)$(INCDIR)/nmond
	$(INSTALL) $(TARGET) $(DESTDIR)$(LIBDIR)/
	ln -s $(TARGET) $(DESTDIR)$(LIBDIR)/$(SONAME)
	ln -s $(SONAME) $(DESTDIR)$(LIBDIR)/$(LIBNAME)
	$(INSTALL) $(NAME).h $(DESTDIR)$(INCDIR)/nmond/
