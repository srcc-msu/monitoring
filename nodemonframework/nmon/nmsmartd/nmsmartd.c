#define _BSD_SOURCE
#define _XOPEN_SOURCE

#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <nm_syslog.inc.c>

#include <libsmart.h>

#include <nm_module.h>
#include <nm_modshm.h>


struct smart_record_t {
	uint8_t		err;
	uint8_t		val;
	uint8_t		worst;
	uint8_t		thresh;
	uint8_t		raw[6];
	uint8_t		padding[2];
} __attribute__((packed));

#define SMART_ELM_LEN	sizeof(struct smart_record_t)


#define SMATTR_TO_SATTR(x)	(x & 0xFF)
#define SATTR_TO_SMATTR(x)	((uint16_t)x | 0x0A00)

/************ WARNINNG!!! ************/
/* Changing one string is NOT allowed - you MUST change whole block!!! */

#define IDX_READ_ERR	0
#define IDX_RELLOC_CNT	1
#define IDX_SEEK_ERR	2
#define IDX_SPIN_RETR	3
#define IDX_TEMP	4

#define IDX_NUM		5


static inline uint16_t idx_to_msattr(uint8_t idx){
	switch(idx){
	case IDX_READ_ERR: return MON_SMART_RAW_READ_ERROR_RATE;
	case IDX_RELLOC_CNT: return MON_SMART_REALLOCATED_SECTOR_COUNT;
	case IDX_SEEK_ERR: return MON_SMART_SEEK_ERROR_RATE;
	case IDX_SPIN_RETR: return MON_SMART_SPIN_RETRY_COUNT;
	case IDX_TEMP: return MON_SMART_TEMPERATURE;
	}
	
	/* This code should never run */
	
	return 0xFFFF;
}


static inline uint8_t sattr_to_idx(uint8_t sattr){
	switch(SATTR_TO_SMATTR(sattr)){
	case MON_SMART_RAW_READ_ERROR_RATE: return IDX_READ_ERR;
	case MON_SMART_REALLOCATED_SECTOR_COUNT: return IDX_RELLOC_CNT;
	case MON_SMART_SEEK_ERROR_RATE: return IDX_SEEK_ERR;
	case MON_SMART_SPIN_RETRY_COUNT: return IDX_SPIN_RETR;
	case MON_SMART_TEMPERATURE: return IDX_TEMP;
	}
	
	/* This code should never run */
	
	return 0;
}

static inline int supported_smattr(uint16_t smattr){
	if (smattr == MON_SMART_RAW_READ_ERROR_RATE ||
	    smattr == MON_SMART_REALLOCATED_SECTOR_COUNT ||
	    smattr == MON_SMART_SEEK_ERROR_RATE ||
	    smattr == MON_SMART_SPIN_RETRY_COUNT ||
	    smattr == MON_SMART_TEMPERATURE)
		return 1;
	else
		return 0;
}

/************* END OF BLOCK ***********/


struct smartmod_data_t {
	uint8_t		num;
	int		*disks_fd;
	uint8_t		*dynval;
};


static void init_smart_hdrs(struct smartmod_data_t *buf){
	int i;
	
	for (i=0; i<IDX_NUM; i++){
		*(struct nm_tlv_hdr_t *)NM_GROUPADDR(buf->dynval, SMART_ELM_LEN, buf->num, i) = 
		    NM_MONTYPE(idx_to_msattr(i), SMART_ELM_LEN * buf->num);
	}
}


static void cleanup_smartbuf(struct smartmod_data_t *buf, struct nm_module_bufdesc_t *mdesc){
	int i;

	if (buf->disks_fd){
		for (i=0; i < buf->num && buf->disks_fd[i]; i++)
			close(buf->disks_fd[i]);
		free(buf->disks_fd);
		buf->disks_fd = NULL;
	}
	buf->num = 0;

	nm_mod_buf_dt(buf->dynval, mdesc);
	buf->dynval = NULL;
}


static int precheck_smartdisks(char *disks){
	int fd;
	int di = 0;
	int err = 0;
	char *dname;
	char *d = strdup(disks);
	
	while ((dname = strtok(di ? NULL : d, ","))){
		if ((fd = open(dname, O_RDONLY)) < 0){
			err = -1;
			break;
		}
		
		if (smart_enabled(fd) || smart_support(fd)){
			err = -2;
			break;
		}
		
		close(fd);
		di++;
	}
	
	free(d);
	return err;
}


static int prepare_smartbuf(char *disks, struct smartmod_data_t *buf, struct nm_module_bufdesc_t *mdesc){
	int dsknum, i;
	char *tmp = disks;
	size_t dynlen;
	
	dsknum = i = 0;
	
	while ((tmp = strchr(tmp, ','))){
		dsknum++;
		tmp++;
	}
	dsknum++;
	
	if (!(buf->disks_fd = malloc(sizeof(int) * dsknum))){
		return -1;
	}
	bzero(buf->disks_fd, sizeof(int) * dsknum);
	buf->num = dsknum;
	
	while ((tmp = strtok(i ? NULL : disks, ","))){
		if ((buf->disks_fd[i] = open(tmp, O_RDONLY)) < 0){
			if (!mdesc)
				fprintf(stderr, "Failed to open device %s!\n", tmp);
			goto err_xit;
		}
		
		if (smart_enabled(buf->disks_fd[i]) || smart_support(buf->disks_fd[i])){
			if (!mdesc)
				fprintf(stderr, "No SMART support or SMART not enabled in %s!\n", tmp);
			goto err_xit;
		}
		
		i++;
	}
	
	dynlen = NM_DYNELEMLEN(SMART_ELM_LEN, dsknum) * IDX_NUM;
	buf->dynval = nm_mod_buf_at(dynlen, mdesc);
	if (!buf->dynval)
		goto err_xit;

	bzero(buf->dynval, dynlen);
	init_smart_hdrs(buf);
	
	return 0;
	
err_xit:
	cleanup_smartbuf(buf, mdesc);
	return -1;
}


static void usage(FILE *s){
	fprintf(s, APPNAME " [options]\n"
	    "\t-h		: show this message\n"
	    "\t-s key		: module SHM key\n"
	    "\t-d disks	: list of disk devices\n"
	    "\t-c config	: config file\n\n");
}


static void read_smarts(struct smartmod_data_t *info, int is_module){
	uint64_t raw;
	int i, j, k, ts, tsm, err;
	struct ata_smart_values vals;
	struct ata_smart_thresholds_pvt thrsh;
	
	bzero(&vals, sizeof(struct ata_smart_values));
	bzero(&thrsh, sizeof(struct ata_smart_thresholds_pvt));
		
	for (i=0; i < info->num; i++){
#define SVADDR(I)	NM_VECTADDR(info->dynval, SMART_ELM_LEN, info->num, I, struct smart_record_t)
		if (smart_values(info->disks_fd[i], &vals) || smart_thresholds(info->disks_fd[i], &thrsh)){
			//Got error
			err = 0xff;
			
			for (j=0; j<IDX_NUM; j++){
				SVADDR(j)[i].err = 0xff;
			}
			
			if (!is_module){
				printf("Disk %3d:\tERROR!\n", i);
			}
		} else {
			//OK
			err = 0;
			
			for (j=0; j < NUMBER_ATA_SMART_ATTRIBUTES; j++){
				ts = vals.vendor_attributes[j].id;
				tsm = SATTR_TO_SMATTR(ts);
				
				if (supported_smattr(tsm)){
					SVADDR(sattr_to_idx(ts))[i].err = 0;
					SVADDR(sattr_to_idx(ts))[i].val = vals.vendor_attributes[j].current;
					SVADDR(sattr_to_idx(ts))[i].worst = vals.vendor_attributes[j].worst;
					SVADDR(sattr_to_idx(ts))[i].thresh = thrsh.thres_entries[j].threshold;
					
					for (k = 0; k < 6; k++){
						SVADDR(sattr_to_idx(ts))[i].raw[k] = vals.vendor_attributes[j].raw[5-k];
					}
					
					if (!is_module){
						raw = 0;
						memcpy(&raw, vals.vendor_attributes[j].raw, 6);
						printf("Disk %3d Attr: %3d:\tVAL: %d\tWORST: %d\tTHRSH: %d\tRaw: %lu\n", i, ts,
						    vals.vendor_attributes[j].current, vals.vendor_attributes[j].worst, thrsh.thres_entries[j].threshold,
						    raw);
					}
				}
			}
		}
#undef SVADDR
	}
}


static int smart_active = 1;


static void smart_read_loop(struct smartmod_data_t *buf){
	while (smart_active){
		read_smarts(buf, 1);
		/*
		 * Should be less than NM_STAT_PERIOD_SEC 
		 * but statistics period is quite long.
		 * So we lost a few (m)seconds
		 */
		sleep(NM_STAT_GET_PERIOD_SEC);
	}
}


void smart_deactivate(int sig){
	smart_active = 0;
}


int main(int ac, char *av[]){
	int opt;
	FILE *f;
	char *arg, *val;
	int shmkey = -1;
	char *disks = NULL;
	char *config = NULL;
	struct nm_module_bufdesc_t *bufdesc = NULL;
	struct smartmod_data_t sdata;
	struct sigaction sigact;
	
	nm_syslog(LOG_DEBUG, "%s", "Staring");
	
	while ((opt = getopt(ac, av, "hs:d:c:")) != -1){
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
		arg = malloc(BUFSIZE);
		val = malloc(BUFSIZE);
		
		if (!(arg && val)){
			_exit(4);
		}
		
		if (!(f = fopen(config, "r"))){
			fprintf(stderr, "Config file not found!\n");
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
		fprintf(stderr, "No disks specified. Exiting\n");
		nm_syslog(LOG_ERR, "%s", "no disks specified");
		_exit(8);
	}
	
	if (precheck_smartdisks(disks) < 0){
		fprintf(stderr, "Wrong disk, no smart support or smart disabled.\n");
		nm_syslog(LOG_ERR, "%s", "Wrong disk, no smart support or smart disabled.");
		_exit(8);
	}
	
	bzero(&sdata, sizeof(struct smartmod_data_t));
	
	if (shmkey != -1){
		bufdesc = nm_mod_bufdesc_at(shmkey);
		if (!bufdesc){
			nm_syslog(LOG_ERR, "%s", "cannot commuticate with master process!");
			_exit(4);
		}
	}
	
	if (prepare_smartbuf(disks, &sdata, bufdesc) < 0){
		nm_syslog(LOG_ERR, "%s", "cannot sync with master process!");
		_exit(4);
	}
	
	nm_syslog(LOG_NOTICE, "%s", "starting S.M.A.R.T. monitor");
	if (shmkey != -1){
		signal(SIGHUP, SIG_IGN);
		signal(SIGINT, SIG_IGN);
		bzero(&sigact, sizeof(struct sigaction));
		sigact.sa_handler = smart_deactivate;
		sigemptyset(&sigact.sa_mask);
		sigaction(SIGTERM, &sigact, NULL);
		smart_read_loop(&sdata);
	} else
		read_smarts(&sdata, 0);
	
	nm_syslog(LOG_NOTICE, "%s", "stopping S.M.A.R.T. monitor");
	
	cleanup_smartbuf(&sdata, bufdesc);
	nm_mod_bufdesc_dt(bufdesc);
	free(disks);
	
	return 0;
}


