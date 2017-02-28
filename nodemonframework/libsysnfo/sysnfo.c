#define _GNU_SOURCE

#ifdef TINYSYSNFO
#define _BSD_SOURCE
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <libsmart.h>

#include <sysnfo.h>


#define DEVMEM		"/dev/mem"
#define NETDIR		"/sys/class/net"
#define DEVDIR		"/dev"

#define DEF_ARR_SIZE	256

#ifdef _DEBUG
#define DBG(format...)	fprintf(stderr, format)
#else
#define DBG(format...)
#endif


struct dmi_hdr_t{
	uint8_t 	dmi_type;
	uint8_t 	dmi_len;
	uint8_t 	*dmi_data;
};


static const char *dmi_string(struct dmi_hdr_t *dh, uint8_t s){
	char *p = (char *) dh->dmi_data;
	
	if (!s)
		return NULL;
		
	p += dh->dmi_len;
	while (s > 1 && *p){
		p += strlen(p) + 1;
		s--;
	}
	
	if (! *p)
		return NULL;
	
	return p;
}


#ifndef TINYSYSNFO
static const char *dmi_ipmi_interface_type(uint8_t code)
{
	/* 3.3.39.1 and IPMI 2.0, appendix C1, table C1-2 */
	static const char *type[] = {
		"Unknown", /* 0x00 */
		"KCS (Keyboard Control Style)",
		"SMIC (Server Management Interface Chip)",
		"BT (Block Transfer)",
		"SSIF (SMBus System Interface)" /* 0x04 */
	};

	if (code <= 0x04)
		return type[code];
	return NULL;
}
#endif


static void dmi_decode(struct dmi_hdr_t *dh, struct sys_info *si){
#ifndef TINYSYSNFO
	int i;
#endif
	int unused;
	switch (dh->dmi_type){
#ifndef TINYSYSNFO
	case 0:	/* BIOS */
		if (dh->dmi_len < 0x12)
			break;
		unused = asprintf(&si->si_smbios.si_vendor, "%s", dmi_string(dh, dh->dmi_data[0x04]));
		unused = asprintf(&si->si_vendbios.vi_rev, "%s", dmi_string(dh, dh->dmi_data[0x05]));
		unused = asprintf(&si->si_vendbios.vi_reldate, "%s", dmi_string(dh, dh->dmi_data[0x08]));
		
		if (dh->dmi_len < 0x18)
			break;
		if (dh->dmi_data[0x14] != 0xFF && dh->dmi_data[0x15] != 0xFF){
			unused = asprintf(&si->si_smbios.si_version, "%u.%u", dh->dmi_data[0x14], dh->dmi_data[0x15]);
		}
		break;
#endif
	case 2:	/* Motherboard*/
		if (dh->dmi_len < 0x08)
			break;
#ifndef TINYSYSNFO
		unused = asprintf(&si->si_vendbios.vi_vendor, "%s", dmi_string(dh, dh->dmi_data[0x04]));
#endif
		unused = asprintf(&si->si_mb.mi_vendor, "%s", dmi_string(dh, dh->dmi_data[0x04]));
		unused = asprintf(&si->si_mb.mi_product, "%s", dmi_string(dh, dh->dmi_data[0x05]));
		unused = asprintf(&si->si_mb.mi_version, "%s", dmi_string(dh, dh->dmi_data[0x06]));
		unused = asprintf(&si->si_mb.mi_version, "%s", dmi_string(dh, dh->dmi_data[0x07]));
		break;
#ifndef TINYSYSNFO
	case 4:	/* Processor */
		if (dh->dmi_len < 0x1A)
			break;
		si->si_cpu.ci_vendor = strdup(dmi_string(dh, dh->dmi_data[0x07]));
		si->si_cpu.ci_family = (dh->dmi_data[0x06] == 0xFE && dh->dmi_len >= 0x2A) ?
		    *((uint16_t *)(dh->dmi_data + 0x28)) : dh->dmi_data[0x06];
		si->si_cpu.ci_model = strdup(dmi_string(dh, dh->dmi_data[0x10]));
		si->si_cpu.ci_speed = *((uint16_t *)(dh->dmi_data+0x14));
		si->si_cpu.ci_clock = *((uint16_t *)(dh->dmi_data+0x12));
		if (dh->dmi_len < 0x28){
			si->si_cpu.ci_amount++;
			break;
		} else {
			si->si_cpu.ci_amount += dh->dmi_data[0x23];
		}
		break;
// NOTE: Probably get memory information in other way
	case 17:/* Memory */
		if (dh->dmi_len < 0x15)
			break;
		si->si_mem.mi_type = dh->dmi_data[0x12];
		for (i=0; i<DEF_ARR_SIZE && si->si_mem.mi_banks[i]; i++);
		unused = asprintf(&(si->si_mem.mi_banks[i]), "%u", *((uint16_t *)(dh->dmi_data+0x0C)));
		si->si_mem.mi_amount_mb += *((uint16_t *)(dh->dmi_data+0x0C));
		break;

	case 38:/* IPMI */
		if (dh->dmi_len < 10)
			break;
		si->si_ipmi.ii_type = strdup(dmi_ipmi_interface_type(dh->dmi_data[0x04]));
		unused = asprintf(&si->si_ipmi.ii_specver, "%u.%u", dh->dmi_data[0x05] >> 4, dh->dmi_data[0x05] & 0x0F);
		break;
#endif
	}
}


static void dmi_getinfo(struct sys_info *si){
	int fd, off;
	void *buf;
	uint8_t *addr, *data, *next;
	uint16_t dlen, dnum;
	int i;
	struct dmi_hdr_t dhdr;
	uint32_t base = 0;

	if ((fd = open(DEVMEM, O_RDONLY)) < 0)
		return;

	if ((buf = mmap(0, 0x10000, PROT_READ, MAP_SHARED, fd, 0xF0000)) == MAP_FAILED)
		goto map_fail;

	for (off = 0; off <= 0xFFE0; off += 16){
		addr = (uint8_t *) (buf + off);
		if (!memcmp(buf + off, "_SM_", 4)){
			//DMI len
			dlen = *((uint16_t *) (addr + 0x16));
			dnum = *((uint16_t *) (addr + 0x1c));
			//DMI Base address
			base = *((uint32_t *) (addr + 0x18));
			
			break;
		}
	}

	munmap(buf, 0x10000);
	if (!base){
		//base addr not found
		goto map_fail;
	}

	if ((buf = mmap(0, (base % sysconf(_SC_PAGESIZE)) + dlen, PROT_READ, MAP_SHARED, fd, base - (base % sysconf(_SC_PAGESIZE)))) == MAP_FAILED){
		goto map_fail;
	}

	addr = buf + (base % sysconf(_SC_PAGESIZE));
	data = addr;
	i = 0;
	while (i < dnum && data + 4 <= addr + dlen){
		dhdr.dmi_type = data[0];
		dhdr.dmi_len = data[1];
		dhdr.dmi_data = data;
		if (dhdr.dmi_len < 4){
			//error
			goto err;
		}
		
		if (dhdr.dmi_type == 127)
			break;	//end of table
		
		next = data + dhdr.dmi_len;
		while (next - addr + 1 < dlen && (next[0] != 0 || next[1] != 0))
			next++;
		next += 2;
		
		if (next - addr <= dlen)
			dmi_decode(&dhdr, si);
		else {
			break;
		}
//??? dmi_table_string
		
		data = next;
		i++;
	}

//TODO: error checking

err:
	munmap(buf, (base % sysconf(_SC_PAGESIZE)) + dlen);
map_fail:
	close(fd);
//	return err;
}


#ifndef TINYSYSNFO
static void net_getinfo(struct sys_info *si){
	int ieth, iib;
	FILE *ifaddr;
	DIR *netdir;
	struct dirent *ifdir;
	char *addrname, *addr;
	
//	err = 0;
	ieth = iib = 0;
	
	if (!(netdir = opendir(NETDIR)))
		return;
	
	if (!(addrname = malloc(sysconf(_SC_PAGESIZE)))){
//		err = -2;
		goto nomem;
	}
	
	if (!(addr = malloc(256))){
//		err = -2;
		goto nomemb;
	}
	
	while ((ifdir = readdir(netdir))){
		if (strstr(ifdir->d_name, "eth") == ifdir->d_name){
			//Ethernet
			bzero(addrname, sysconf(_SC_PAGESIZE));
			sprintf(addrname, NETDIR "/%s/address", ifdir->d_name);
			if (!(ifaddr = fopen(addrname, "r"))){
//				err = -3;
				break;
			}
			bzero(addr, 256);
			addr = fgets(addr, 256, ifaddr);
			addr[strlen(addr) - 1] = 0;
			si->si_eth[ieth++] = strdup(addr);
			fclose(ifaddr);
		} else if (strstr(ifdir->d_name, "ib") == ifdir->d_name){
			//Infiniband
			bzero(addrname, sysconf(_SC_PAGESIZE));
			sprintf(addrname, NETDIR "/%s/address", ifdir->d_name);
			if (!(ifaddr = fopen(addrname, "r"))){
//				err = -3;
				break;
			}
			bzero(addr, 256);
			addr = fgets(addr, 256, ifaddr);
			addr[strlen(addr) - 1] = 0;
			si->si_ib[iib++] = strdup(addr);
			fclose(ifaddr);
		}
	}
	
	free(addr);
nomemb:
	free(addrname);
nomem:
	closedir(netdir);
	
//	return err;
}


static int dsk_getinfo(struct sys_info *si){
	int err, dskfd, indx;
	DIR *devdir;
	struct dirent *dsk;
	char *dskname;
	struct ata_identify_device ident;
	
	err = 0;
	indx = 0;
	
	if (!(devdir = opendir(DEVDIR)))
		return -1;
	
	if (!(dskname = malloc(sysconf(_SC_PAGESIZE)))){
		err = -2;
		goto nomem;
	}
	
	while ((dsk = readdir(devdir))){
		if (strstr(dsk->d_name, "sd") == dsk->d_name && !isdigit(dsk->d_name[strlen(dsk->d_name)-1])){
			bzero(dskname, sysconf(_SC_PAGESIZE));
			sprintf(dskname, DEVDIR "/%s", dsk->d_name);
			if ((dskfd = open(dskname, O_RDONLY)) < 0){
				continue;
			}
			if (!smart_identify(dskfd, &ident)){
				si->si_hd[indx] = malloc(40);
				bzero(si->si_hd[indx], 40);
				strncpy(si->si_hd[indx], (char *) ident.model, 40);
				indx++;
			}
			close(dskfd);
		}
	}
	
	free(dskname);
nomem:
	closedir(devdir);
	
	return err;
}


static void free_arr(char **arr){
	int i;
	
	for (i=0; i < DEF_ARR_SIZE && arr[i]; i++)
		free(arr[i]);
	
	free(arr);
}
#endif	/* TINYSYSNFO*/


void free_sys_info(struct sys_info *si){
#ifndef TINYSYSNFO
	free(si->si_cpu.ci_model);
	free(si->si_cpu.ci_vendor);
	
	free_arr(si->si_mem.mi_banks);
#endif
	free(si->si_mb.mi_vendor);
	free(si->si_mb.mi_product);
	free(si->si_mb.mi_version);
	free(si->si_mb.mi_id);
#ifndef TINYSYSNFO
	free(si->si_smbios.si_vendor);
	free(si->si_smbios.si_version);
	free(si->si_smbios.si_reldate);
	free(si->si_smbios.si_rev);
	
	free(si->si_vendbios.vi_vendor);
	free(si->si_vendbios.vi_type);
	free(si->si_vendbios.vi_reldate);
	free(si->si_vendbios.vi_rev);
	
	free(si->si_ipmi.ii_type);
	free(si->si_ipmi.ii_specver);
	
	free_arr(si->si_hd);
	free_arr(si->si_ib);
	free_arr(si->si_eth);
#endif
	free(si);
}


struct sys_info *init_sys_info(){
	struct sys_info *si = malloc(sizeof(struct sys_info));
	bzero(si, sizeof(struct sys_info));
#ifndef TINYSYSNFO
	si->si_mem.mi_banks = malloc(DEF_ARR_SIZE);
	si->si_hd = malloc(DEF_ARR_SIZE);
	si->si_ib = malloc(DEF_ARR_SIZE);
	si->si_eth = malloc(DEF_ARR_SIZE);

	bzero(si->si_mem.mi_banks, DEF_ARR_SIZE);
	bzero(si->si_hd, DEF_ARR_SIZE);
	bzero(si->si_ib, DEF_ARR_SIZE);
	bzero(si->si_eth, DEF_ARR_SIZE);
#endif
	dmi_getinfo(si);
#ifndef TINYSYSNFO
	net_getinfo(si);
	dsk_getinfo(si);
#endif
	return si;
}


