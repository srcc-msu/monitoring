#define _BSD_SOURCE
#define _XOPEN_SOURCE

#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <nm_module.h>
#include <nm_modshm.h>
#include <nm_syslog.inc.c>


#define UPTIME_FILE	"/proc/uptime"
#define DISKSTATS_FILE	"/proc/diskstats"

#define BPS_ELM_LEN	sizeof(uint64_t)
#define IOPS_ELM_LEN	sizeof(uint32_t)

#define IDX_READ	0
#define IDX_WRITE	1

#define IDX_NUM		2


#define NAMELEN		32


struct io_stats {
	/* # of sectors read */
	unsigned long long rd_sectors	__attribute__ ((aligned (8)));
	/* # of sectors written */
	unsigned long long wr_sectors	__attribute__ ((packed));
	/* # of read operations issued to the device */
	unsigned long rd_ios		__attribute__ ((packed));
	/* # of read requests merged */
	unsigned long rd_merges		__attribute__ ((packed));
	/* Time of read requests in queue */
	unsigned long rd_ticks		__attribute__ ((packed));
	/* # of write operations issued to the device */
	unsigned long wr_ios		__attribute__ ((packed));
	/* # of write requests merged */
	unsigned long wr_merges		__attribute__ ((packed));
	/* Time of write requests in queue */
	unsigned long wr_ticks		__attribute__ ((packed));
	/* # of I/Os in progress */
	unsigned long ios_pgr		__attribute__ ((packed));
	/* # of ticks total (for this device) for I/O */
	unsigned long tot_ticks		__attribute__ ((packed));
	/* # of ticks requests spent in queue */
	unsigned long rq_ticks		__attribute__ ((packed));
	/* # of I/O done since last reboot */
	unsigned long dk_drive		__attribute__ ((packed));
	/* # of blocks read */
	unsigned long dk_drive_rblk	__attribute__ ((packed));
	/* # of blocks written */
	unsigned long dk_drive_wblk	__attribute__ ((packed));
};


struct io_data_t {
	uint8_t		num;
	FILE		*fp;
	char		**disks;
	struct io_stats	*prev;
	uint8_t		*dynval;
	uint8_t		*iops;
};


static void init_io_hdrs(struct io_data_t *buf){
	int i;
	
	for (i=0; i<IDX_NUM; i++){
		*(struct nm_tlv_hdr_t *)NM_GROUPADDR(buf->dynval, BPS_ELM_LEN, buf->num, i) = 
		    NM_MONTYPE(MON_IO_BPS_READ + i, BPS_ELM_LEN * buf->num);
		
		*(struct nm_tlv_hdr_t *)NM_GROUPADDR(buf->iops, IOPS_ELM_LEN, buf->num, i) = 
		    NM_MONTYPE(MON_IO_IOPS_READ + i, IOPS_ELM_LEN * buf->num);
	}
}


static void cleanup_iobuf(struct io_data_t *buf, struct nm_module_bufdesc_t *mdesc){
	if (buf->disks){
		free(buf->disks);
		buf->disks = NULL;
	}
	buf->num = 0;
	
	if (buf->prev){
		free(buf->prev);
		buf->prev = NULL;
	}

	nm_mod_buf_dt(buf->dynval, mdesc);
	buf->dynval = NULL;
	
	if (buf->fp){
		fclose(buf->fp);
		buf->fp = NULL;
	}
}


static int check_dsk(char *dsk, struct io_data_t *buf){
	char name[NAMELEN];

	fseek(buf->fp, 0, SEEK_SET);

	while (!feof(buf->fp)){
		if (fscanf(buf->fp, "%*u %*u %s %*[^\r\n]", name) == 1){
			if (!strncmp(dsk, name, NAMELEN))
				return 0;
		}
	}

	return -1;
}


static int prepare_iobuf(char *dsks, struct io_data_t *buf, struct nm_module_bufdesc_t *mdesc){
	int dsknum, i;
	char *tmp = dsks;
	size_t dynlen;
	
	dsknum = i = 0;
	
	if (!(buf->fp = fopen(DISKSTATS_FILE, "r"))){
		if (!mdesc)
			perror("fopen");
		else
			nm_syslog(LOG_ERR, "fopen: %s", strerror(errno));
	}
	
	while ((tmp = strchr(tmp, ','))){
		dsknum++;
		tmp++;
	}
	dsknum++;
	
	if (!(buf->disks = malloc(sizeof(char *) * dsknum))){
		if (!mdesc)
			perror("malloc");
		else
			nm_syslog(LOG_ERR, "malloc: %s", strerror(errno));

		return -1;
	}
	bzero(buf->disks, sizeof(char *) * dsknum);
	buf->num = dsknum;
	
	if (!(buf->prev = malloc(sizeof(struct io_stats) * dsknum))){
		if (!mdesc)
			perror("malloc");
		else
			nm_syslog(LOG_ERR, "malloc: %s", strerror(errno));

		goto err_xit;
	}
	bzero(buf->prev, sizeof(struct io_stats) * dsknum);
	
	while ((tmp = strtok(i ? NULL : dsks, ","))){
		if (check_dsk(tmp, buf) < 0){
			if (!mdesc)
				fprintf(stderr, "Wrong disk device %s!\n", tmp);
			else
				nm_syslog(LOG_ERR, "Wrong disk device %s!", tmp);

			goto err_xit;
		}
		
		buf->disks[i] = tmp;
		
		i++;
	}
	
	dynlen = (NM_DYNELEMLEN(BPS_ELM_LEN, dsknum) + NM_DYNELEMLEN(IOPS_ELM_LEN, dsknum)) * IDX_NUM;
	buf->dynval = nm_mod_buf_at(dynlen, mdesc);
	if (!buf->dynval){
		nm_syslog(LOG_ERR, "%s", "cannot initialize buffer for sensors");
		goto err_xit;
	}
	
	buf->iops = buf->dynval + (NM_DYNELEMLEN(BPS_ELM_LEN, dsknum) * IDX_NUM);
	
	bzero(buf->dynval, dynlen);
	init_io_hdrs(buf);
	
	return 0;
	
err_xit:
	cleanup_iobuf(buf, mdesc);
	return -1;
}


static void usage(FILE *s){
	fprintf(s, APPNAME " [options]\n"
	    "\t-h		: show this message\n"
	    "\t-s key		: module SHM key\n"
	    "\t-d disks	: list of disk devices\n"
	    "\t-c config	: config file\n\n");
}


static inline uint64_t htonll(uint64_t val){
#ifndef __BIG_ENDIAN__
	uint64_t res;
	res = (((uint64_t)htonl(val >> 32)) & 0xFFFFFFFF) | 
	    (((uint64_t)(htonl(val & 0xFFFFFFFF))) << 32);
	return res;
#else
	return val;
#endif
}


#define S_VAL(x, y)	((y) - (x))
static uint64_t norm_val(uint64_t v0, uint64_t v1){
	if ((v1 < v0) && (v0 <= 0xffffffff))
		return ((v1 - v0) & 0xffffffff);
	else
		return S_VAL(v0, v1);
}


static void read_io(struct io_data_t *info, int is_module){
	unsigned int i, unused;
	char buf[BUFSIZE];
	char name[NAMELEN];
	struct io_stats stats;
	uint32_t rd_iops, wr_iops;
	uint64_t rd_bps, wr_bps;
	
	fseek(info->fp, 0, SEEK_SET);
	
	while (fgets(buf, BUFSIZE, info->fp)){
		bzero(name, NAMELEN);
		sscanf(buf, "%u %u %s %lu %lu %llu %lu %lu %lu %llu %lu %lu %lu %lu",
		    &unused, &unused, name,
		    &stats.rd_ios, &stats.rd_merges, &stats.rd_sectors, &stats.rd_ticks,
		    &stats.wr_ios, &stats.wr_merges, &stats.wr_sectors, &stats.wr_ticks,
		    &stats.ios_pgr, &stats.tot_ticks, &stats.rq_ticks
		    );
		
		for (i = 0; i < info->num; i++){
#define BPSVADDR(I)	NM_VECTADDR(info->dynval, BPS_ELM_LEN, info->num, I, uint64_t)
#define IOPSVADDR(I)	NM_VECTADDR(info->iops, IOPS_ELM_LEN, info->num, I, uint32_t)
			if (!strncmp(name, info->disks[i], NAMELEN)){
				rd_iops = S_VAL(info->prev[i].rd_ios, stats.rd_ios);
				wr_iops = S_VAL(info->prev[i].wr_ios, stats.wr_ios);
				
				rd_bps = norm_val(info->prev[i].rd_sectors, stats.rd_sectors) * 512;
				wr_bps = norm_val(info->prev[i].wr_sectors, stats.wr_sectors) * 512;
				
				//TODO: fill in buffer
				BPSVADDR(IDX_READ)[i] = htonll(rd_bps);
				BPSVADDR(IDX_WRITE)[i] = htonll(wr_bps);
				
				IOPSVADDR(IDX_READ)[i] = htonl(rd_iops);
				IOPSVADDR(IDX_WRITE)[i] = htonl(wr_iops);
				
				if (!is_module){
					printf("%s: rd_ios: %u wr_ios: %u rd_bps: %lu wr_bps: %lu\n",
					    name, rd_iops, wr_iops, rd_bps, wr_bps);
				}
				
				memcpy(&info->prev[i], &stats, sizeof(struct io_stats));
			}
#undef IOPSVADDR
#undef BPSVADDR
		}
	}
}
#undef S_VAL


static int io_active = 1;


static void io_read_loop(struct io_data_t *buf){
	while (io_active){
		read_io(buf, 1);
		
		sleep(NM_MON_PERIOD_SEC);
	}
}


void io_deactivate(int sig){
	io_active = 0;
}


int main(int ac, char *av[]){
	int opt;
	FILE *f;
	char *arg, *val;
	int shmkey = -1;
	char *disks = NULL;
	char *config = NULL;
	struct nm_module_bufdesc_t *bufdesc = NULL;
	struct io_data_t iodata;
	struct sigaction sigact;
	
	nm_syslog(LOG_DEBUG, "%s", "Staring");
	
	while ((opt = getopt(ac, av, "hd:s:c:")) != -1){
		switch (opt){
		case 'h':/* show help */
			usage(stdout);
			break;
		case 's':/* module SHM key */
			shmkey = atoi(optarg);
			break;
		case 'd':/* disks list */
			disks = strdup(optarg);
			break;
		case 'c':/* config file */
			config = strdup(optarg);
			break;
		default:
			usage(stderr);
			nm_syslog(LOG_ERR, "%s", "bad parameters");
			_exit(8);
		}
	}
	
	if (!disks && !config){
		config = CONFIGFILE;
	}
	
	if (config){
		if (!(arg = malloc(BUFSIZE))){
			perror("malloc");
			nm_syslog(LOG_ERR, "malloc: %s", strerror(errno));
			_exit(4);
		}

		if (!(val = malloc(BUFSIZE))){
			perror("malloc");
			nm_syslog(LOG_ERR, "malloc: %s", strerror(errno));
			_exit(4);
		}
		
		if (!(f = fopen(config, "r"))){
			perror("Cannot open config file");
			nm_syslog(LOG_ERR, "Cannot open config file %s: %s",
				config, strerror(errno));
			_exit(8);
		}
		
		while (!feof(f)){
			fscanf(f, "%s %s", arg, val);
			
			if (!strcmp(arg, "")){
				//Empty string - do nothing
			} else if (!strcmp(arg, "disks")){
				if (!disks)
					disks = strdup(val);
			} else {
				fprintf(stderr, "Bad config file key: %s\n", arg);
				nm_syslog(LOG_ERR, "%s", "bad config");
				_exit(8);
			}
		}
		
		fclose(f);
		free(val);
		free(arg);
		
		if (config != (char *) CONFIGFILE)
			free(config);
	}
	
	if (!disks){
		fprintf(stderr, "No disk devices specified. Exiting\n");
		nm_syslog(LOG_ERR, "%s", "no disk devices");
		_exit(8);
	}
	
	bzero(&iodata, sizeof(struct io_data_t));
	
	if (shmkey != -1){
		bufdesc = nm_mod_bufdesc_at(shmkey);
		if (!bufdesc){
			nm_syslog(LOG_ERR,
				"Cannot commuticate with master process!");
			_exit(4);
		}
	}
	
	if (prepare_iobuf(disks, &iodata, bufdesc) < 0){
		nm_syslog(LOG_ERR,"%s", "initialise IO monitor failed");
		_exit(4);
	}
	
	nm_syslog(LOG_NOTICE, "%s", "starting IO monitor");
	if (shmkey != -1){
		signal(SIGHUP, SIG_IGN);
		signal(SIGINT, SIG_IGN);
		bzero(&sigact, sizeof(struct sigaction));
		sigact.sa_handler = io_deactivate;
		sigemptyset(&sigact.sa_mask);
		sigaction(SIGTERM, &sigact, NULL);
		io_read_loop(&iodata);
	} else {
		read_io(&iodata, 1);
		sleep(1);
		read_io(&iodata, 0);
	}
	
	nm_syslog(LOG_NOTICE, "%s", "stopping IO monitor");
	
	cleanup_iobuf(&iodata, bufdesc);
	nm_mod_bufdesc_dt(bufdesc);
	free(disks);
	
	return 0;
}


