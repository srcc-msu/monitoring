src:=$(wildcard *.c)
obj:=$(patsubst %c, %o, $(src))

tgt_suffix:=$(shell basename `pwd`)
LOCAL_PREFIX=\"$(tgt_suffix)\"

ifeq '$(tgt_suffix)' "smart"
LIBS += -lsmart
endif


TARGET=lib$(PLUGIN_PREFIX)$(tgt_suffix).so

CFLAGS=-O2 -g -pipe -Wall -fPIC -DLOCAL_PREFIX=$(LOCAL_PREFIX) $(IDIR)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(obj)
	$(CC) -shared $^ -o $@ $(LIBS) $(LDIR)

clean:
	$(RM) $(obj) $(TARGET)
