#define _GNU_SOURCE
#define _BSD_SOURCE
#define _XOPEN_SOURCE

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>

#include <ipmi_intf.h>
#include <ipmi_sdr.h>

#include <ipmi_platform.h>
#include <nm_module.h>
#include <nm_modshm.h>
#include <nm_syslog.inc.c>

#define TINYSYSNFO
#include <sysnfo.h>

//TODO: TO BE CHANGED!!!
uint16_t isens_cnt = 0;
uint16_t dynlen = 0;
struct nm_isens_desc_t *isens_desc = NULL;
void *tlvdata = NULL;

struct nmi_platform_desc_t {
	const char		*vendor_name;
	const char		*product_name;
	const char		*firmware_name;
};
//


const struct nmi_platform_desc_t nmi_platforms[] = {
	{"Supermicro", "X8DTT", "nmi_supermicro_x8dtt.idf"},
	{"Supermicro", "X8DTT-IBX", "nmi_supermicro_x8dtt.idf"},
	{"Intel Corporation", "S5520UR", "nmi_intel_s5520ur.idf"},
	{"Intel", "S5000PSL", "nmi_intel_s5000psl.idf"},
	{"T-Platforms", "B2.0-MM", "nmi_tplatforms_b20mm.idf"},
	{"FUJITSU", "D2860", "nmi_fujitsu_d2860.idf"},
	{NULL, NULL, NULL}
};


static int nmi_detect_platform(void){
	int i = 0;
	struct sys_info *si = init_sys_info();
	
	while (nmi_platforms[i].vendor_name){
		if (!strcmp(nmi_platforms[i].vendor_name, si->si_mb.mi_vendor) || 
		    !strcmp(nmi_platforms[i].product_name, si->si_mb.mi_product)){
			free_sys_info(si);
			return i;
		}
		
		i++;
	}
	
	free_sys_info(si);
	return -1;
}


static void nmi_unload_platform(struct nm_module_bufdesc_t *mdesc){
	isens_cnt = 0;
	dynlen = 0;
	
	if (isens_desc){
		free(isens_desc);
		isens_desc = NULL;
	}

	nm_mod_buf_dt(tlvdata, mdesc);
	tlvdata = NULL;
}


static int nmi_load_platform(const char *pdesc, struct nm_module_bufdesc_t *mdesc){
	int pfd = open(pdesc, O_RDONLY);
	
	if (pfd < 0)
		return -1;
	
	if (read(pfd, &isens_cnt, sizeof(uint16_t)) < 0)
		goto err_xit;
	
	if (read(pfd, &dynlen, sizeof(uint16_t)) < 0)
		goto err_xit;
	
	if (!(isens_desc = malloc(isens_cnt * sizeof(struct nm_isens_desc_t))))
		goto err_xit;

	tlvdata = nm_mod_buf_at(dynlen, mdesc);
	if (!tlvdata)
		goto err_xit;

	if (read(pfd, isens_desc, isens_cnt * sizeof(struct nm_isens_desc_t)) < 0)
		goto err_xit;
	
	if (read(pfd, tlvdata, dynlen) < 0)
		goto err_xit;
	
	close(pfd);
	
	return 0;
err_xit:
	close(pfd);
	nmi_unload_platform(mdesc);
	return -1;
}


static int running = 1;

void ipmi_deactivate(int unused){
	running = 0;
}


void nmi_help(FILE *f){
	fprintf(f, "nmipmid [options]\n"
	    "\t-h\t: show this massage\n"
	    "\t-s key\t: module SHM key\n\n"
	);
}


int main(int ac,  char *av[]){
	int opt, pidx;
	int shmid = -1;
	char *pfw_fn;
	struct ipmi_intf *intf;
	struct sigaction sigact;
	struct nm_module_bufdesc_t *bufdesc = NULL;
	
	nm_syslog(LOG_DEBUG, "%s", "Staring");
	
	while ((opt = getopt(ac, av, "hs:")) != -1){
		switch (opt){
		case 'h':
			nmi_help(stdout);
			return 0;
		case 's':
			shmid = atoi(optarg);
			break;
		default: 
			nmi_help(stderr);
			nm_syslog(LOG_ERR, "%s", "bad parameters");
			return 8;
		}
	}
	
	if (shmid == -1){
		nmi_help(stderr);
		nm_syslog(LOG_ERR, "%s", "bad parameters");
		return 8;
	}
	
	intf = ipmi_intf_load(NULL);
	
	if (!intf){
		nm_syslog(LOG_ERR, "%s", "ipmi device unreachable");
		return 1;
	}
	if (!(intf->opened)){
		if (intf->open(intf) < 0){
			nm_syslog(LOG_ERR, "%s", "ipmi device unreachable");
			return 1;
		}
	}
	
	srand(time(NULL));
	
	pidx = nmi_detect_platform();
	
	if (pidx == -1){
		nm_syslog(LOG_ERR, "%s", "platform detection faied!");
		return 1;
	}
	
	if (shmid != -1){
		bufdesc = nm_mod_bufdesc_at(shmid);
		if (!bufdesc){
			nm_syslog(LOG_ERR, "%s", "cannot commuticate with master process!");
			return 4;
		}
	}
	
	asprintf(&pfw_fn, IDFDIR  "%s", nmi_platforms[pidx].firmware_name);
	
	pidx = nmi_load_platform(pfw_fn, bufdesc);
	
	free(pfw_fn);
	
	if (pidx == -1){
		nm_syslog(LOG_ERR, "%s", "platform load failure!");
		nmi_unload_platform(bufdesc);
		nm_mod_bufdesc_dt(bufdesc);
		return 1;
	}
	
	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	bzero(&sigact, sizeof(struct sigaction));
	sigact.sa_handler = ipmi_deactivate;
	sigemptyset(&sigact.sa_mask);
	sigaction(SIGTERM, &sigact, NULL);
	
	nm_syslog(LOG_NOTICE, "%s", "staring IPMI HW monitor");
	while (running){
		ipmi_sdr_print_sdr(intf, SDR_RECORD_TYPE_FULL_SENSOR);
		sleep(NM_MON_PERIOD_SEC);
	}
	ipmi_cleanup(intf);
	
	nm_syslog(LOG_NOTICE, "%s", "stopping IPMI HW monitor");
	
	if (intf->opened)
		intf->close(intf);
	
	nmi_unload_platform(bufdesc);
	nm_mod_bufdesc_dt(bufdesc);
	
	return 0;
}


