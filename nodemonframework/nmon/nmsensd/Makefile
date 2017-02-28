IDIR += -I$(shell pwd)
CFLAGS=-O2 -g -pipe -Wall -std=c99 $(IDIR)
HDFDIR=$(SHDIR)/nmsensd/
PLATFORMS=-DNMS_SUPERMICRO_X7DBT -DNMS_SUPERMICRO_X7DWT -DNMS_SUPERMICRO_X8DTT -DNMS_SUPERMICRO_X8DTT_IBX -DNMS_TYAN_5370 -DNMS_TYAN_5382 -DNMS_TYAN_7029 -DNMS_TYAN_7029MM

hdf_dir=hdf

.PHONY: all clean nms_crc32 sign

all: sign nmsensd

nmsensd: nmsensd.o nms_crc32.o
	${DIET} $(CC) $(STATIC) -o $@ $^ ../nm_modshm.o $(LDIR) -ltinysysnfo

nmsensd.o: nmsensd.c
	${DIET} $(CC) $(CFLAGS) $(PLATFORMS) -DAPPNAME=\"nmsensd\" -DHDFDIR=\"$(HDFDIR)\" -c -o $@ $^

nms_crc32.o: nms_crc32.c
	${DIET} $(CC) $(CFLAGS) -c -o $@ $^

sign: nms_crc32
	IDIR="$(IDIR)" $(MAKE) -C $(hdf_dir)
	for f in $(hdf_dir)/*.hdf; do ./nms_crc32 -s $$f; done

nms_crc32: nms_crc32_app.o
	$(CC) $(CFLAGS) -o $@ $^

nms_crc32_app.o: nms_crc32.c
	$(CC) -c $(CFLAGS) -o $@ $< -DAPPNAME=\"nms_crc32\" -DNMS_CRC32_APP

clean:
	$(MAKE) -C $(hdf_dir) clean
	$(RM) *.o nms_crc32 nmsensd


install:
	$(MAKE) -C $(hdf_dir) install
	$(INSTALL) -m 0755 -d $(DESTDIR)/$(BINDIR)
	$(INSTALL) -m 0755 nmsensd $(DESTDIR)/$(BINDIR)/


