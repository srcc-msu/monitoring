#define _GNU_SOURCE
#define _BSD_SOURCE

#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TINYSYSNFO
#include <sysnfo.h>

#include <i2c-dev.h>
#include <nm_platforms.h>
#include <nms_crc32.h>

#include <nm_module.h>
#include <nm_modshm.h>
#include <nm_syslog.inc.c>

#if defined(NMS_TYAN_5370) || defined(NMS_TYAN_5382)
#include <nms_customs.h>
#endif


#define I2C_FILE	"/dev/i2c-0"
#define I2C_ALT_FILE	"/dev/i2c/0"


struct sens_data_t {
	uint16_t		num;
	int			i2c_fd;
	struct nm_sensor_desc_t	*conf;
	struct nm_platform_internal_t	*idx_data;
	uint8_t			*dynval;
};


static int ident_platform(const struct nm_platform_desc_t *pd){
	int i = 0;
	struct sys_info *si = init_sys_info();
	
	if (!si)
		return -1;
	
	while (pd[i].vendor_name){
		if (!strcmp(pd[i].vendor_name, si->si_mb.mi_vendor) &&
		    !strcmp(pd[i].product_name, si->si_mb.mi_product))
		{
			goto good_xit;
		}
		
		i++;
	}
	
	i = -1;
	
good_xit:
	free_sys_info(si);
	return i;
}


static void inc_sensor_cnt(uint16_t sens_id, struct nm_sensor_cnt_t *scnt){
	uint8_t i = 0;
	
	while (scnt[i].mon_type){
		if (scnt[i].mon_type == sens_id){
			scnt[i].count++;
			return;
		}
		
		i++;
	}
}


static void count_platform_sensors(uint16_t num, struct nm_sensor_cnt_t *scnt,
					    struct nm_sensor_desc_t *sdesc)
{
	uint16_t i;
	
	for (i=0; i < num; i++){
		inc_sensor_cnt(sdesc[i].sensor_id, scnt);
	}
}


static inline int lookup_int_idx(uint16_t montype, uint8_t idx, uint16_t num, struct nm_sensor_desc_t *conf){
	int i;
	
	for (i=0; i < num; i++){
		if (conf[i].sensor_id == montype && conf[i].sensor_index == idx)
			return i;
	}
	
	return -1;
}


static void cleanup_sensbuf(struct sens_data_t *sd, struct nm_module_bufdesc_t *mdesc){
	sd->num = 0;
	
	if (sd->idx_data){
		free(sd->idx_data);
		sd->idx_data = NULL;
	}
	
	if (sd->conf){
		free(sd->conf);
		sd->conf = NULL;
	}

	nm_mod_buf_dt(sd->dynval, mdesc);
	sd->dynval = NULL;
}


static int init_sensbuf(struct nm_sensor_cnt_t *sc, struct sens_data_t *sd, struct nm_module_bufdesc_t *mdesc){
	int i, j, dynlen, idx;
	uint8_t *curr_ptr;
	
	i = dynlen = 0;
	
	while (sc[i].mon_type){
		if (sc[i].count){
			dynlen += sizeof(struct nm_tlv_hdr_t) + sc[i].count * sizeof(uint16_t);
		}
		
		i++;
	}
	sd->dynval = nm_mod_buf_at(dynlen, mdesc);
	if (!sd->dynval)
		goto err_xit;
	
	bzero(sd->dynval, dynlen);
	curr_ptr = sd->dynval;
	i = 0;
	while (sc[i].mon_type){
		if (sc[i].count){
			*(struct nm_tlv_hdr_t *) curr_ptr = NM_MONTYPE(sc[i].mon_type, sc[i].count * sizeof(uint16_t));
			curr_ptr += sizeof(struct nm_tlv_hdr_t);
			
			for (j=0; j < sc[i].count; j++){
				idx = lookup_int_idx(sc[i].mon_type, j, sd->num, sd->conf);
				if (idx < 0){
					/* Paranoia */
					goto err_xit;
				}
				
				sd->idx_data[idx].addr = (uint16_t *) curr_ptr;
				curr_ptr += sizeof(uint16_t);
			}
		}
		
		i++;
	}
	
	return 0;
	
err_xit:
	cleanup_sensbuf(sd, mdesc);
	return -1;
}


static int prepare_sensbuf(const struct nm_platform_desc_t *pd, struct sens_data_t *sd, struct nm_module_bufdesc_t *mdesc){
	char *hdf_fn;
	int fwfd, i, pi;
	int err = 0;
	uint16_t num = 0;
	struct nm_sensor_cnt_t sens_cnt[]=
	{
		{MON_SYS_TEMP, 0},
		{MON_CPU_TEMP, 0},
		{MON_CPU_VCORE, 0},
		{MON_V_3_3, 0},
		{MON_V_5, 0},
		{MON_V_12, 0},
		{MON_V_N12, 0},
		{MON_V_1_2, 0},
		{MON_V_1_4, 0},
		{MON_V_1_5, 0},
		{MON_V_3_3VSB, 0},
		{MON_V_5VSB, 0},
		{MON_V_BAT, 0},
		{MON_CPU_FAN, 0},
		{MON_SYS_FAN, 0},
		{MON_V_DIMM, 0},
		{0, 0}
	};
	
	if ((pi = ident_platform(pd)) < 0)
		return -1;
	
	if (asprintf(&hdf_fn, HDFDIR "%s", pd[pi].firmware_name) < 0)
		return -1;
	
	fwfd = open(hdf_fn, O_RDONLY);
	free(hdf_fn);
	
	if (fwfd < 0)
		return -1;
	
	if (!nms_valid_crc32(fwfd)){
		err = -1;
		goto err_xit;
	}
	
	lseek(fwfd, 0, SEEK_SET);
	read(fwfd, &num, sizeof(uint16_t));
	if (num != NMSENSD_FW_VERSION){
		err = -1;
		goto err_xit;
	}
	
	read(fwfd, &num, sizeof(uint16_t));
	if (!(sd->conf = malloc(num * sizeof(struct nm_sensor_desc_t)))){
		cleanup_sensbuf(sd, mdesc);
		err = -1;
		goto err_xit;
	}
	
	if (!(sd->idx_data = malloc(num * sizeof(struct nm_platform_internal_t)))){
		cleanup_sensbuf(sd, mdesc);
		err = -1;
		goto err_xit;
	}
	bzero(sd->idx_data, num * sizeof(struct nm_platform_internal_t));
	
	for (i=0; i<num; i++){
		read(fwfd, &(sd->conf[i]), sizeof(struct nm_sensor_desc_t));
	}
	sd->num = num;
	
	count_platform_sensors(num, sens_cnt, sd->conf);
	
	if (init_sensbuf(sens_cnt, sd, mdesc) < 0){
		cleanup_sensbuf(sd, mdesc);
		err = -1;
	}
	
	if (pd[pi].init_call)
		pd[pi].init_call(sd->num, sd->idx_data, sd->conf);
	
err_xit:
	close(fwfd);
	return err;
}


static int smsc_fan_tach(int fd, unsigned char LSBReg){
	union {
		uint8_t b[2];
		uint16_t w;
	} data;
	int reading;
	
	data.b[0] = i2c_smbus_read_byte_data(fd, LSBReg) & 0xFF;
	data.b[1] = i2c_smbus_read_byte_data(fd, LSBReg + 1) & 0xFF;
	
	if ((data.w == 0xFFFF) || (data.w == 0))
		reading = 0;
	else 
		reading = 90000 / data.w;
	
	if (reading > 255)
		reading = 255;
	
	return reading;
}


static int w83793_fan_tach(int fd, unsigned char LSBReg){
	union {
		uint8_t b[2];
		uint16_t w;
	} data;
	int reading;

	data.b[1] = i2c_smbus_read_byte_data(fd, LSBReg - 1) & 0xFF;
	data.b[0] = i2c_smbus_read_byte_data(fd, LSBReg) & 0xFF;

	if ((data.w == 0x0FFF) || (data.w == 0))
		return 0;

	reading = 22500 / data.w;

	if (reading < 6)
		reading = 0;
	else if (reading > 255)
		reading = 255;

	return reading;
}


static int w83627_fan_tach(int fd, unsigned char LSBReg){
	unsigned char data;
	int reading;
	
	data = i2c_smbus_read_byte_data(fd, LSBReg);
	
	if (data == 0xFF || data == 0x00 || data == 0x01)
		reading = 0;
	else 
		reading = 14144 / data;
		
	return reading;
}


static void process_platform(int fd, uint16_t num,
				struct nm_sensor_desc_t *sens,
				struct nm_platform_internal_t *idesc,
				int module)
{
	uint8_t err = 0;
	uint16_t i;
	int32_t tres;
	uint16_t val = 0;
	
	for (i=0; i<num; i++){
		if (err){
			if (idesc[i].addr)
				*(idesc[i].addr) = 0;
			
			continue;
		}
		
		if (nm_sens_is_mgmt(sens[i].sensor_type)){
			switch (sens[i].sensor_type){
			case NM_SENS_TYPE_MGMT_WRITE:
				if (i2c_smbus_write_byte_data(fd, sens[i].sensor_index, sens[i].sensor_address) == -1)
					err = 0xff;
				break;
			case NM_SENS_TYPE_MGMT_BANK:
				if (ioctl(fd, I2C_SLAVE_FORCE, sens[i].sensor_address) < 0)
					err = 0xff;
				break;
			}
			
			continue;
		}
		
		if (!idesc[i].read_call){
			switch (sens[i].sensor_type){
			case NM_SENS_TYPE_DISCRETE:
				if ((tres = i2c_smbus_read_byte_data(fd, sens[i].sensor_address)) < 0)
					val = 0;
				else
					val = tres & 0xFFFF;
				break;
			case NM_SENS_TYPE_SMSCTACH: 
				val = smsc_fan_tach(fd, sens[i].sensor_address);
				break;
			case NM_SENS_TYPE_W83793TACH:
				val = w83793_fan_tach(fd, sens[i].sensor_address);
				break;
			case NM_SENS_TYPE_W83627TACH:
				val = w83627_fan_tach(fd, sens[i].sensor_address);
				break;
			}
			
			val *= sens[i].sensor_divisor;
		} else {
			val = idesc[i].read_call(fd, sens[i].sensor_index, sens[i].sensor_divisor);
		}
		
		if (module){
			printf("Sensor: %d[%d]\tValue: %d\n", sens[i].sensor_id, sens[i].sensor_index, val);
		}
		
		*(idesc[i].addr) = htons(val);
	}
}


static int sensors_running = 1;


void sensors_deactivate(int sig){
	sensors_running = 0;
}


static void sensors_read_loop(int fd, struct sens_data_t *sd){
	while (sensors_running){
		process_platform(fd, sd->num, sd->conf, sd->idx_data, 0);
		sleep(NM_MON_PERIOD_SEC);
	}
}


static void print_usage(FILE *s){
	fprintf(s, APPNAME " [options]\n"
	    "\t-h\t: show this message\n"
	    "\t-s key\t: module SHM key\n\n"
	    );
}


int main(int ac, char *av[]){
	int opt, err;
	int shmid = -1;
	long funcs;
	struct sigaction sigact;
	struct sens_data_t sdata;
	struct nm_module_bufdesc_t *bufdesc = NULL;
	
	const struct nm_platform_desc_t nm_platforms[] = {
#ifdef NMS_SUPERMICRO_X7DBT
		{"Supermicro", "X7DBT", "nms_supermicro_x7dbt.hdf", NULL},
#endif
#ifdef NMS_SUPERMICRO_X7DWT
		{"Supermicro", "X7DWT", "nms_supermicro_x7dwt.hdf", NULL},
#endif
#ifdef NMS_SUPERMICRO_X8DTT
		{"Supermicro", "X8DTT", "nms_supermicro_x8dtt.hdf", NULL},
#endif
#ifdef NMS_SUPERMICRO_X8DTT_IBX
		{"Supermicro", "X8DTT-IBX", "nms_supermicro_x8dtt_ibx.hdf", NULL},
#endif
#ifdef NMS_TYAN_5370
		{"TYAN Corporation", "TYAN-Tempest-i5000VF-S5370", "nms_tyan_5370.hdf", nms_t5370_init},
#endif
#ifdef NMS_TYAN_5382
		{"TYAN Corporation", "TYAN-Tempest-i5000VF-S5382", "nms_tyan_5382.hdf", nms_t5382_init},
#endif
#ifdef NMS_TYAN_7029
		{"T-Platforms", "B2.0-MB-NHM", "nms_tyan_7029.hdf", NULL},
#endif
#ifdef NMS_TYAN_7029MM
		{"T-Platforms", "B2.0-MM", "nms_tyan_7029mm.hdf", NULL},
#endif
		{NULL, NULL, NULL, NULL}
	};
	
	nm_syslog(LOG_DEBUG, "%s", "Staring");
	
	err = 0;
	bzero(&sdata, sizeof(struct sens_data_t));
	
	while ((opt = getopt(ac, av, "hs:")) != -1){
		switch (opt){
		case 'h':
			print_usage(stdout);
			return 0;
		case 's':
			shmid = atoi(optarg);
			break;
		default:
			print_usage(stderr);
			nm_syslog(LOG_ERR, "%s", "bad parameters");
			return 8;
		}
	}
	
	if ((sdata.i2c_fd = open(I2C_FILE, O_RDWR)) < 0){
		if ((sdata.i2c_fd = open(I2C_FILE, O_RDWR)) < 0){
			fprintf(stderr, "Cannot open i2c bus!\n");
			nm_syslog(LOG_ERR, "%s", "i2c bus unreachable");
			return 4;
		}
	}
	
	if (ioctl(sdata.i2c_fd, I2C_FUNCS, &funcs) < 0){
		fprintf(stderr, "Could not get adapter functionality matrix\n");
		nm_syslog(LOG_ERR, "%s", "i2c bus: cannot get funcs");
		err = 4;
		goto i2c_err;
	}
	
	if (!(funcs & I2C_FUNC_SMBUS_READ_BYTE_DATA)){
		fprintf(stderr, "Adapter has no byte read capability\n");
		nm_syslog(LOG_ERR, "%s", "i2c bus: bo byte read capability");
		err = 4;
		goto i2c_err;
	}
	
	if (shmid != -1){
		bufdesc = nm_mod_bufdesc_at(shmid);
		if (!bufdesc){
			nm_syslog(LOG_ERR, "%s", "cannot commuticate with master process!");
			err = 4;
			goto i2c_err;
		}
	}
	
	if (prepare_sensbuf(nm_platforms, &sdata, bufdesc)){
		fprintf(stderr, "Error init platform data!\n");
		nm_syslog(LOG_ERR, "%s", "platform init failed!");
		err = 4;
		goto shm_err;
	}
	
	nm_syslog(LOG_NOTICE, "%s", "starting i2c HW monitor");
	if (bufdesc){
		signal(SIGHUP, SIG_IGN);
		signal(SIGINT, SIG_IGN);
		bzero(&sigact, sizeof(struct sigaction));
		sigact.sa_handler = sensors_deactivate;
		sigemptyset(&sigact.sa_mask);
		sigaction(SIGTERM, &sigact, NULL);
		sensors_read_loop(sdata.i2c_fd, &sdata);
	} else {
		process_platform(sdata.i2c_fd, sdata.num, sdata.conf, sdata.idx_data, 1);
	}
	
	nm_syslog(LOG_NOTICE, "%s", "stopping i2c HW monitor");
	
	cleanup_sensbuf(&sdata, bufdesc);
shm_err:
	nm_mod_bufdesc_dt(bufdesc);
i2c_err:
	close(sdata.i2c_fd);
	
	return 0;
}



