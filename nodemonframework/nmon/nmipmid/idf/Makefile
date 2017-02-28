LD=ld
LDFLAGS=-T idf.ld
CFLAGS=-O0 -pipe $(IDIR)

SRCS:=$(wildcard *.c)
OBJS:=$(patsubst %c, %o, $(SRCS))
TGTS:=$(patsubst %o, %idf, $(OBJS))

.PHONY = all clean install

all: $(TGTS)


clean:
	$(RM) *.o *.idf

%.idf: %.o
	$(LD) $(LDFLAGS) -o $@ $<


install:
	$(INSTALL) -m 0755 -d $(DESTDIR)/$(SHDIR)/nmipmid
	$(INSTALL) -m 0644 *.idf $(DESTDIR)/$(SHDIR)/nmipmid/

