#define _GNU_SOURCE
#define _XOPEN_SOURCE

#include <arpa/inet.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <nm_syslog.inc.c>
#include <nm_module.h>
#include <nm_modshm.h>

#include <nmond/nmib.h>


#define IB_ELM_LEN	sizeof(uint64_t)


struct ib_data_t {
	short int	num_ifs;
	nmib_attr_t	*attr_ifs;
	nmib_t		**ifs;
	uint8_t		*dynval;
};


enum {
	SYMBOL_ERR_IDX = 0,
	VL15_DROPPED_IDX,
	EXCESSIVE_BUF_OVERRUN_IDX,
	LINK_DOWNED_IDX,
	LINK_ERR_RECOVER_IDX,
	LOCAL_LINK_INTEGRITY_ERR_IDX,
	RECV_CONSRTAINT_ERR_IDX,
	XMIT_CONSRTAINT_ERR_IDX,
	RECV_DATA_IDX,
	XMIT_DATA_IDX,
	RECV_PACKETS_IDX,
	XMIT_PACKETS_IDX,
	RECV_ERR_IDX,
	RECV_REM_PHYS_ERR_IDX,
	RECV_SW_RELAY_ERR_IDX,
	XMIT_DISCARDS_IDX,
	XMIT_WAIT_IDX,

	IB_SENSORS_NUM
};


#define SYMBOL_ERR_F			symbolerrors
#define VL15_DROPPED_F			vl15dropped
#define EXCESSIVE_BUF_OVERRUN_F		excbufoverrunerrors
#define LINK_DOWNED_F			linkdowned
#define LINK_ERR_RECOVER_F		linkrecovers
#define LOCAL_LINK_INTEGRITY_ERR_F	linkintegrityerrors
#define RECV_CONSRTAINT_ERR_F		rcvconstrainterrors
#define XMIT_CONSRTAINT_ERR_F		xmtconstrainterrors
#define RECV_DATA_F			rcvdata
#define XMIT_DATA_F			xmtdata
#define RECV_PACKETS_F			rcvpkts
#define XMIT_PACKETS_F			xmtpkts
#define RECV_ERR_F			rcverrors
#define RECV_REM_PHYS_ERR_F		rcvremotephyerrors
#define RECV_SW_RELAY_ERR_F		rcvswrelayerrors
#define XMIT_DISCARDS_F			xmtdiscards
#define XMIT_WAIT_F			xmtwait


static void init_ib_hdrs(struct ib_data_t *data){
#define S(SENS)	*(struct nm_tlv_hdr_t *)\
	NM_GROUPADDR(data->dynval, IB_ELM_LEN, data->num_ifs, SENS ## _IDX) = \
	NM_MONTYPE(MON_IB_P_ ## SENS, IB_ELM_LEN * data->num_ifs)
	S(SYMBOL_ERR);
	S(VL15_DROPPED);
	S(EXCESSIVE_BUF_OVERRUN);
	S(LINK_DOWNED);
	S(LINK_ERR_RECOVER);
	S(LOCAL_LINK_INTEGRITY_ERR);
	S(RECV_CONSRTAINT_ERR);
	S(XMIT_CONSRTAINT_ERR);
	S(RECV_DATA);
	S(XMIT_DATA);
	S(RECV_PACKETS);
	S(XMIT_PACKETS);
	S(RECV_ERR);
	S(RECV_REM_PHYS_ERR);
	S(RECV_SW_RELAY_ERR);
	S(XMIT_DISCARDS);
	S(XMIT_WAIT);
#undef S
}


static void cleanup_ibbuf(struct ib_data_t *data, struct nm_module_bufdesc_t *mdesc){
	int i;

	if (data->ifs){
		for (i = 0; i < data->num_ifs; i++){
			if (data->ifs[i]){
				nmib_close(data->ifs[i]);
			}
		}
		free(data->ifs);
		data->ifs = NULL;
	}
	data->num_ifs = 0;

	if (data->attr_ifs){
		free(data->attr_ifs);
		data->attr_ifs = NULL;
	}

	if (data->dynval){
		nm_mod_buf_dt(data->dynval, mdesc);
		data->dynval = NULL;
	}
}


static int calc_num_ifs(char *ifs_str){
	int num = 0;
	char *c;

	c = ifs_str;
	while ((c = strchr(c, ','))){
		num++;
		c++;
	}
	num++;

	return num;
}


static char *parse_ifs_str(char *ifs_str, nmib_attr_t *attr){
	char *c;
	char *port_num_str = NULL;

	attr->dev_name = c = ifs_str;
	c++;
	while ((*c != '\0') && (*c != ',')){
		if (*c == ':'){
			*c = '\0';
			port_num_str = ++c;
		} else if (*c == '/'){
			nm_syslog(LOG_ERR, "cannot parse interface '%s': "
					"invalid character '/'", ifs_str);
			return NULL;
		}
		c++;
	}
	if (*c == ','){
		*c = '\0';
		c++;
	}

	if (attr->dev_name == '\0'){
		nm_syslog(LOG_ERR, "cannot parse interface '%s': "
					"empty device name", ifs_str);
		return NULL;
	}

	if (port_num_str == NULL){
		nm_syslog(LOG_ERR, "cannot parse interface '%s':"
					"empty number of port", ifs_str);
		return NULL;
	}

	if (*port_num_str == '\0'){
		nm_syslog(LOG_ERR, "cannot parse interface '%s':"
					"empty number of port", ifs_str);
		return NULL;
	}

	attr->port = atoi(port_num_str);
	return c;
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
#define ntohll(x)	htonll(x)


static int prepare_ibbuf(char *ifs_str, struct ib_data_t *data,
			 struct nm_module_bufdesc_t *mdesc){
	int i, active_ifs_num, s_idx;
	size_t dynlen;
	char *pos;
	nmib_attr_t *attr;
	nmib_t *iface;

	data->ifs = NULL;
	data->attr_ifs = NULL;
	data->dynval = NULL;

	data->num_ifs = calc_num_ifs(ifs_str);

	if (!(data->attr_ifs = malloc(data->num_ifs * sizeof(nmib_attr_t)))){
		nm_syslog(LOG_ERR, "prepare_ibbuf: "
				"cannot allocate memory: %s", strerror(errno));
		return -1;
	}

	if (!(data->ifs = malloc(data->num_ifs * sizeof(nmib_t *)))){
		nm_syslog(LOG_ERR, "prepare_ibbuf: "
				"cannot allocate memory: %s", strerror(errno));
		return -1;
	}

	for (i = 0; i < data->num_ifs; i++){
		data->ifs[i] = NULL;
	}

	active_ifs_num = 0;
	pos = ifs_str;
	for (i = 0; i < data->num_ifs; i++){
		attr = &data->attr_ifs[i];

		pos = parse_ifs_str(pos, attr);
		if (!pos){
			return -1;
		}

		if (!(data->ifs[i] = nmib_open(attr))){
			nm_syslog(LOG_ERR, "prepare_ibbuf: "
					"cannot open '%s' port '%d'",
					attr->dev_name, attr->port);
		} else {
			active_ifs_num++;
		}
	}

	if (!active_ifs_num){
		nm_syslog(LOG_ERR, "%s", "prepare_ibbuf: "
				"failed to open any of the interfaces");
		return -1;
	}

	dynlen = NM_DYNELEMLEN(IB_ELM_LEN, data->num_ifs) * IB_SENSORS_NUM;
	data->dynval = nm_mod_buf_at(dynlen, mdesc);
	if (!data->dynval){
		return -1;
	}

	memset(data->dynval, 0, dynlen);
	init_ib_hdrs(data);

	for (i = 0; i < data->num_ifs; i++){
#define IBVADDR(I) NM_VECTADDR(data->dynval, IB_ELM_LEN, data->num_ifs, I,\
								uint64_t)
		iface = data->ifs[i];

		if (!(iface)){
			for (s_idx = 0; s_idx < IB_SENSORS_NUM; s_idx++){
				IBVADDR(s_idx)[i] = htonll((uint64_t)NM_CNT_ERR);
			}
			continue;
		}
		if (!(iface->flags & NMIB_IFACE_XMTWAIT)){
			IBVADDR(XMIT_WAIT_IDX)[i] = htonll((uint64_t)NM_CNT_NONE);
		}
#undef IBVADDR
	}
	return 0;
}


void print_sensors(nmib_t *iface){
	nmib_counters_t *cntrs = &iface->counters;
#define S(SENS) printf("%s = %llu\n", # SENS,\
			(long long unsigned)cntrs->SENS ## _F)
	S(SYMBOL_ERR);
	S(VL15_DROPPED);
	S(EXCESSIVE_BUF_OVERRUN);
	S(LINK_DOWNED);
	S(LINK_ERR_RECOVER);
	S(LOCAL_LINK_INTEGRITY_ERR);
	S(RECV_CONSRTAINT_ERR);
	S(XMIT_CONSRTAINT_ERR);
	S(RECV_DATA);
	S(XMIT_DATA);
	S(RECV_PACKETS);
	S(XMIT_PACKETS);
	S(RECV_ERR);
	S(RECV_REM_PHYS_ERR);
	S(RECV_SW_RELAY_ERR);
	S(XMIT_DISCARDS);
	if (iface->flags & NMIB_IFACE_XMTWAIT){
		S(XMIT_WAIT);
	}
#undef S
}


static void read_ib(struct ib_data_t *data, short int is_module){
	int i, s_idx;
	nmib_t *iface;
	nmib_attr_t *attr;
	nmib_counters_t *cntrs;

	for (i = 0; i < data->num_ifs; i++){
#define IBVADDR(I) NM_VECTADDR(data->dynval, IB_ELM_LEN, data->num_ifs, I,\
								uint64_t)
		iface = data->ifs[i];
		if (!iface)
			continue;

		attr = &data->attr_ifs[i];
		if (nmib_read_counters(iface)){
			nm_syslog(LOG_ERR,
				"cannot read sensors from '%s' port '%d'",
					attr->dev_name, attr->port);
			nmib_close(iface);
			data->ifs[i] = NULL;
			for (s_idx = 0; s_idx < IB_SENSORS_NUM; s_idx++){
				IBVADDR(s_idx)[i] = htonll((uint64_t)NM_CNT_ERR);
			}
			continue;
		}
		cntrs = &iface->counters;

		/* InfiniBand Architecture Specification 16.1.3.5
		   PortXmitData, PortRcvData - total number of data octets,
		   divided by 4. */
		cntrs->xmtdata = cntrs->xmtdata << 2;
		cntrs->rcvdata = cntrs->rcvdata << 2;

		if (!is_module){
			printf("interface '%s' port '%d' sensors:\n",
					attr->dev_name, attr->port);
			print_sensors(iface);
		} else {
#define S(SENS) IBVADDR(SENS ## _IDX)[i] =\
				htonll(NM_GET_CNT64(cntrs-> SENS ## _F))
			S(SYMBOL_ERR);
			S(VL15_DROPPED);
			S(EXCESSIVE_BUF_OVERRUN);
			S(LINK_DOWNED);
			S(LINK_ERR_RECOVER);
			S(LOCAL_LINK_INTEGRITY_ERR);
			S(RECV_CONSRTAINT_ERR);
			S(XMIT_CONSRTAINT_ERR);
			S(RECV_DATA);
			S(XMIT_DATA);
			S(RECV_PACKETS);
			S(XMIT_PACKETS);
			S(RECV_ERR);
			S(RECV_REM_PHYS_ERR);
			S(RECV_SW_RELAY_ERR);
			S(XMIT_DISCARDS);
			S(XMIT_WAIT);
#undef S
		}
#undef IBVADDR
	}
}


static int ib_active = 1;

void ib_deactivate(int sig){
	ib_active = 0;
}


static void ib_read_loop(struct ib_data_t *buf){
	while (ib_active){
		read_ib(buf, 1);

		sleep(NM_MON_PERIOD_SEC);
	}
}


static void usage(FILE *s){
	fprintf(s, APPNAME " [options]\n"
	    "\t-h               : show this message\n"
	    "\t-s key           : module SHM key\n"
	    "\t-i interfaces    : interfaces list\n"
	    "\t-c config        : config file\n\n"
	    "Interfaces list looks like \"dev0_id0:port0,dev0_id1:port2, ...\"\n\n");
}


static int get_conf(int ac, char *av[], int *shmkey, char **ifs){
	int opt;
	FILE *f = NULL;
	char *arg = NULL;
	char *val = NULL;
	char *config = NULL;

	*ifs = NULL;
	*shmkey = -1;

	while ((opt = getopt(ac, av, "hi:s:c:")) != -1){
		switch (opt){
		case 'h':/* show help */
			usage(stdout);
			_exit(0);
		case 's':/* module SHM key */
			*shmkey = atoi(optarg);
			break;
		case 'i':/* interfaces list */
			*ifs = strdup(optarg);
			break;
		case 'c':/* config file */
			config = strdup(optarg);
			break;
		default:
			usage(stderr);
			return 8;
		}
	}

	if (*ifs)
		return 0;

	if (!config)
		config = CONFIGFILE;

	arg = malloc(BUFSIZE);
	val = malloc(BUFSIZE);

	if (!arg || !val){
		fprintf(stderr, "cannot allocate memory\n");
		return 4;
	}

	if (!(f = fopen(config, "r"))){
		fprintf(stderr, "cannot open config file '%s'\n", config);
		return 8;
	}

	while (!feof(f)){
		fscanf(f, "%s %s", arg, val);

		if (!strcmp(arg, "")){
			/* Empty string - do nothing */
		} else if (!strcmp(arg, "interfaces")){
			*ifs = strdup(val);
		} else {
			fprintf(stderr, "bad configuration parameter '%s'",
									arg);
			return 8;
		}
	}

	fclose(f);
	free(val);
	free(arg);

	if (config != (char *) CONFIGFILE)
		free(config);

	if (!(*ifs)){
		fprintf(stderr, "no interfaces\n");
		return 8;
	}

	return 0;
}


int main(int argc, char *argv[]){
	int shmkey;
	int conf_status;
	int rc = 0;
	char *ifs;
	struct nm_module_bufdesc_t *bufdesc = NULL;
	struct ib_data_t ibdata;
	struct sigaction sigact;

	nm_syslog(LOG_DEBUG, "%s", "Staring");

	conf_status = get_conf(argc, argv, &shmkey, &ifs);
	if (conf_status){
		nm_syslog(LOG_ERR, "%s", "read configuration failed");
		_exit(conf_status);
	}

	if (shmkey != -1){
		bufdesc = nm_mod_bufdesc_at(shmkey);
		if (!bufdesc){
			nm_syslog(LOG_ERR, "%s", "cannot commuticate with master process");
			_exit(4);
		}
	}

	memset(&ibdata, 0, sizeof(struct ib_data_t));
	if (prepare_ibbuf(ifs, &ibdata, bufdesc)){
		nm_syslog(LOG_ERR, "%s", "IB monitor initialization failed");
		rc = 4;
		goto err_xit;
	}

	nm_syslog(LOG_NOTICE, "%s", "starting IB monitor");
	if (shmkey != -1){
		signal(SIGHUP, SIG_IGN);
		signal(SIGINT, SIG_IGN);
		memset(&sigact, 0, sizeof(struct sigaction));
		sigact.sa_handler = ib_deactivate;
		sigemptyset(&sigact.sa_mask);
		sigaction(SIGTERM, &sigact, NULL);
		ib_read_loop(&ibdata);
	} else {
		read_ib(&ibdata, 0);
	}

	nm_syslog(LOG_NOTICE, "%s", "stopping IB monitor");

err_xit:
	cleanup_ibbuf(&ibdata, bufdesc);
	nm_mod_bufdesc_dt(bufdesc);

	return rc;
}

