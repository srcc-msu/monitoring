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

#include <sys/vfs.h>
#include <mntent.h>

#include <nm_module.h>
#include <nm_modshm.h>


#define MOUNTS_FILE	"/proc/self/mounts"

#define FS_ELM_LEN	sizeof(uint16_t)

#define IDX_BYTES	0
#define IDX_INODES	1

#define IDX_NUM		2


struct fs_data_t {
	uint8_t		num;
	int		*mp_fds;
	uint8_t		*dynval;
};


static void init_fs_hdrs(struct fs_data_t *buf){
	int i;
	
	for (i=0; i<IDX_NUM; i++){
		*(struct nm_tlv_hdr_t *)NM_GROUPADDR(buf->dynval, FS_ELM_LEN, buf->num, i) = 
		    NM_MONTYPE(MON_FS_BYTES_USAGE + i, FS_ELM_LEN * buf->num);
	}
}


static void cleanup_fsbuf(struct fs_data_t *buf, struct nm_module_bufdesc_t *mdesc){
	int i;

	if (buf->mp_fds){
		for (i=0; i < buf->num && buf->mp_fds[i]; i++)
			close(buf->mp_fds[i]);
		free(buf->mp_fds);
		buf->mp_fds = NULL;
	}
	buf->num = 0;

	nm_mod_buf_dt(buf->dynval, mdesc);
	buf->dynval = NULL;
}


static int check_mntpnt(char *mp){
	int ans = -1;
	FILE *mts;
	struct mntent *mntfield;
	
	mts = setmntent(MOUNTS_FILE, "r");
	while ((mntfield = getmntent(mts))){
		if (!strcmp(mntfield->mnt_dir, mp)){
			ans = 0;
			break;
		}
	}
	
	fclose(mts);
	
	return ans;
}


static int prepare_fsbuf(char *mpts, struct fs_data_t *buf, struct nm_module_bufdesc_t *mdesc){
	int mpnum, i;
	char *tmp = mpts;
	size_t dynlen;
	
	mpnum = i = 0;
	
	while ((tmp = strchr(tmp, ','))){
		mpnum++;
		tmp++;
	}
	mpnum++;
	
	if (!(buf->mp_fds = malloc(sizeof(int) * mpnum))){
		return -1;
	}
	bzero(buf->mp_fds, sizeof(int) * mpnum);
	buf->num = mpnum;
	
	while ((tmp = strtok(i ? NULL : mpts, ","))){
		if ((buf->mp_fds[i] = open(tmp, O_RDONLY)) < 0){
			if (!mdesc)
				fprintf(stderr, "Unable to open path %s!\n", tmp);
			goto err_xit;
		}
		
		if (check_mntpnt(tmp) < 0){
			if (!mdesc)
				fprintf(stderr, "Wrong mountpoint %s!\n", tmp);
			goto err_xit;
		}
		
		i++;
	}
	
	dynlen = NM_DYNELEMLEN(FS_ELM_LEN, mpnum) * IDX_NUM;

	buf->dynval = nm_mod_buf_at(dynlen, mdesc);
        if (!buf->dynval)
		goto err_xit;

	bzero(buf->dynval, dynlen);
	init_fs_hdrs(buf);
	
	return 0;
	
err_xit:
	cleanup_fsbuf(buf, mdesc);
	return -1;
}


static void usage(FILE *s){
	fprintf(s, APPNAME " [options]\n"
	    "\t-h		: show this message\n"
	    "\t-s key		: module SHM key\n"
	    "\t-m mountmoints	: list of mount moints\n"
	    "\t-c config	: config file\n\n");
}


static void read_fs(struct fs_data_t *info, int is_module){
	int i, j;
	uint16_t ifv_bytes, ifv_inodes;
	struct statfs statbuf;
	
	bzero(&statbuf, sizeof(struct statfs));
		
	for (i=0; i < info->num; i++){
#define FSVADDR(I)	NM_VECTADDR(info->dynval, FS_ELM_LEN, info->num, I, uint16_t)
		if (fstatfs(info->mp_fds[i], &statbuf) < 0){
			//Got error
			for (j=0; j<IDX_NUM; j++){
				FSVADDR(j)[i] = 0;
			}
			
			if (!is_module){
				printf("Mountpoint %3d: ERROR!\n", i);
			}
		} else {
			//OK
			if (statbuf.f_blocks && statbuf.f_files){
				ifv_bytes = (uint16_t)(((uint64_t)(statbuf.f_blocks - statbuf.f_bavail) * 25600 / statbuf.f_blocks) + 2560);
				ifv_inodes = (uint16_t)(((uint64_t)(statbuf.f_files - statbuf.f_ffree) * 25600 / statbuf.f_files) + 2560);
			} else {
				ifv_bytes = ifv_inodes = 0;
			}
			
			FSVADDR(IDX_BYTES)[i] = htons(ifv_bytes);
			FSVADDR(IDX_INODES)[i] = htons(ifv_inodes);
			
			if (!is_module){
				printf("Mountpoint %3d:\tIFV_BYTES: %d\tIFV_INODES: %d\n", i, ifv_bytes, ifv_inodes);
			}
		}
#undef FSVADDR
	}
}


static int fs_active = 1;


static void fs_read_loop(struct fs_data_t *buf){
	while (fs_active){
		read_fs(buf, 1);
		/*
		 * Should be less than NM_STAT_PERIOD_SEC 
		 * but statistics period is quite long.
		 * So we lost a few (m)seconds
		 */
		sleep(NM_STAT_SEND_PERIOD_SEC);
	}
}


void fs_deactivate(int sig){
	fs_active = 0;
}


int main(int ac, char *av[]){
	int opt;
	FILE *f;
	char *arg, *val;
	int shmkey = -1;
	char *mounts = NULL;
	char *config = NULL;
	struct nm_module_bufdesc_t *bufdesc = NULL;
	struct fs_data_t fsdata;
	struct sigaction sigact;
	
	nm_syslog(LOG_DEBUG, "%s", "Staring");
	
	while ((opt = getopt(ac, av, "hm:s:c:")) != -1){
		switch (opt){
		case 'h':/* show help */
			usage(stdout);
			break;
		case 's':/* module SHM key */
			shmkey = atoi(optarg);
			break;
		case 'm':/* mounts list */
			mounts = strdup(optarg);
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
	
	if (!mounts && !config){
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
			} else if (!strcmp(arg, "mounts")){
				if (!mounts)
					mounts = strdup(val);
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
	
	if (!mounts){
		fprintf(stderr, "No mount points specified. Exiting\n");
		nm_syslog(LOG_ERR, "%s", "no mountpoints");
		_exit(8);
	}
	
	bzero(&fsdata, sizeof(struct fs_data_t));
	
	if (shmkey != -1){
		bufdesc = nm_mod_bufdesc_at(shmkey);
		if (!bufdesc){
			nm_syslog(LOG_ERR, "%s", "cannot commuticate with master process");
			_exit(4);
		}
	}
	
	if (prepare_fsbuf(mounts, &fsdata, bufdesc) < 0){
		nm_syslog(LOG_ERR, "%s", "cannot sync with master process!");
		_exit(4);
	}
	
	nm_syslog(LOG_NOTICE, "%s", "starting FS monitor");
	if (shmkey != -1){
		signal(SIGHUP, SIG_IGN);
		signal(SIGINT, SIG_IGN);
		bzero(&sigact, sizeof(struct sigaction));
		sigact.sa_handler = fs_deactivate;
		sigemptyset(&sigact.sa_mask);
		sigaction(SIGTERM, &sigact, NULL);
		fs_read_loop(&fsdata);
	} else
		read_fs(&fsdata, 0);
	
	nm_syslog(LOG_NOTICE, "%s", "stopping FS monitor");
	
	cleanup_fsbuf(&fsdata, bufdesc);
	nm_mod_bufdesc_dt(bufdesc);
	free(mounts);
	
	return 0;
}


