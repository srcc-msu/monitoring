#ifndef _NM_DUMB_H_
#define _NM_DUMB_H_

#include <arpa/inet.h>
#include <time.h>

#include <stdint.h>


/********************** Utility routines *************************/
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


/******************* Network interfaces routines *****************/

struct nm_iface_info_t {
	uint8_t 	num;
	char 		**names;
	uint8_t 	*dynval;
	uint16_t	dynlen;
};


int nm_init_ifaces(char *ifsstr, struct nm_iface_info_t *info);
void nm_release_ifaces(struct nm_iface_info_t *info);
int nm_getinfo_ifaces(struct nm_iface_info_t *info);


/************************** CPU routines **************************/

struct nm_cpu_info_t {
	uint8_t 	num;
	uint64_t	**prevval;
	time_t		prev_t;
	uint8_t 	*dynval;
	uint16_t	dynlen;
};


int nm_init_cpus(struct nm_cpu_info_t *info);
void nm_release_cpus(struct nm_cpu_info_t *info);
int nm_getinfo_cpus(struct nm_cpu_info_t *info);


/************************** Mem routines **************************/

struct nm_mem_info_t {
	uint8_t		*dynvalmem;
	uint16_t	dynmlen;
	uint8_t		*dynvalhtlb;
	uint16_t	dynhlen;
	uint8_t		*dynvalvm;
	uint16_t	dynvlen;
};


int nm_init_mem(struct nm_mem_info_t *info);
void nm_release_mem(struct nm_mem_info_t *info);
void nm_getinfo_mem(struct nm_mem_info_t *info);


/******************* HW status routines *********************/

struct nm_hw_status_t {
	uint8_t		code;
	uint16_t	msg_len;
	char		*hw_msg;
	uint16_t	dynlen;
	uint8_t		*dynval;
};

struct nm_hw_opt_t {
	int retr_max;
	int retr_interval;
	int ac;
	char **av;
};


int nm_init_hwstat(struct nm_hw_opt_t *opt, struct nm_hw_status_t *stat);
void nm_cleanup_hwstat(struct nm_hw_status_t *stat);
//TODO: Uncomment in nm_hwstat.c. Reservod for nect releases
//int nm_hwstat_file(char *fn, struct nm_hw_status_t *stat);


#endif
