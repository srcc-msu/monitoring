LD=ld
LDFLAGS=-T hdf.ld
CFLAGS=-O0 -pipe $(IDIR)

SRCS:=$(wildcard *.c)
OBJS:=$(patsubst %c, %o, $(SRCS))
TGTS:=$(patsubst %o, %hdf, $(OBJS))

all: $(TGTS)


clean:
	$(RM) *.o *.hdf


%.hdf: %.o
	$(LD) $(LDFLAGS) -o $@ $<


install:
	$(INSTALL) -m 0755 -d $(DESTDIR)/$(SHDIR)/nmsensd
	$(INSTALL) -m 0644 *.hdf $(DESTDIR)/$(SHDIR)/nmsensd/