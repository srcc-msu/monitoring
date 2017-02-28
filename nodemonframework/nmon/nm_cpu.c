#define _BSD_SOURCE

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nm_dumb.h"
#include "nm_module.h"


#define CPU_FILE	"/proc/stat"
#define MCE_FILE	"/proc/interrupts"
#define FREQ_FILE	"/proc/cpuinfo"
#define AVNRN_FILE	"/proc/loadavg"

#define CPU_ELM_LEN	sizeof(uint16_t)
#define MCE_ELM_LEN	sizeof(uint32_t)
#define AVNRN_ELM_LEN	sizeof(uint32_t)

#define CPU_IDX_USR	0
#define CPU_IDX_NICE	1
#define CPU_IDX_SYS	2
#define CPU_IDX_IDLE	3
#define CPU_IDX_IOW	4
#define CPU_IDX_IRQ	5
#define CPU_IDX_SIRQ	6

#define CPU_IDX_MAX	7

#define AVNRN_IDX_1M	0
#define AVNRN_IDX_5M	1
#define AVNRN_IDX_15M	2

#define AVNRN_IDX_MAX	3

static uint8_t *nm_avnrn_addr = NULL;

#ifdef X86
static uint8_t *nm_freq_addr = NULL;
#endif

#ifdef MCE
static uint8_t *nm_mce_addr = NULL;
#endif

static void init_cpu_hdrs(struct nm_cpu_info_t *info){
	int i;
	
	for (i=0; i<CPU_IDX_MAX; i++){
		*(struct nm_tlv_hdr_t *)NM_GROUPADDR(info->dynval, CPU_ELM_LEN, info->num, i) = 
		    NM_MONTYPE(MON_CPU_USAGE_USER + i, CPU_ELM_LEN * info->num);
	}

	for (i=0; i<AVNRN_IDX_MAX; i++){
		*(struct nm_tlv_hdr_t *)NM_GROUPADDR(nm_avnrn_addr, AVNRN_ELM_LEN, 1, i) =
		    NM_MONTYPE(MON_AVENRUN_1M + i, AVNRN_ELM_LEN);
	}
#ifdef X86
	*(struct nm_tlv_hdr_t *)nm_freq_addr = NM_MONTYPE(MON_CPU_FREQ, CPU_ELM_LEN * info->num);
#endif
#ifdef MCE
	*(struct nm_tlv_hdr_t *)nm_mce_addr = NM_MONTYPE(MON_CPU_MCE_TOTAL, MCE_ELM_LEN);
#endif
}


int nm_init_cpus(struct nm_cpu_info_t *info){
	int i, err, ncpus, tlen, alen;
	FILE *f;
	char *buf;
#ifdef X86
	int flen;
#endif
	err = ncpus = 0;

	if (!info)
		return 0;

	if (!(buf = malloc(BUFSIZE)))
		return -1;

	if (!(f = fopen(CPU_FILE, "r"))){
		err = -1;
		goto err_xit;
	}

	while (fgets(buf, BUFSIZE, f)){
		if (!strncmp(buf, "cpu", 3) && isdigit(buf[3]))
			ncpus++;
	}

	fclose(f);

	info->prevval = malloc(sizeof(uint64_t *) * CPU_IDX_MAX);
	if (!info->prevval){
		err = -1;
		goto err_xit;
	}
	bzero(info->prevval, sizeof(uint64_t *) * CPU_IDX_MAX);
	for (i=0; i<CPU_IDX_MAX; i++){
		info->prevval[i] = malloc(sizeof(uint64_t) * ncpus);
		if (!info->prevval[i]){
			err = -1;
			nm_release_cpus(info);
			goto err_xit;
		}
		bzero(info->prevval[i], sizeof(uint64_t) * ncpus);
	}

	tlen = NM_DYNELEMLEN(CPU_ELM_LEN, ncpus) * CPU_IDX_MAX;

	alen = NM_DYNELEMLEN(AVNRN_ELM_LEN, 1) * AVNRN_IDX_MAX;
	tlen += alen;
#ifdef X86
	flen = NM_DYNELEMLEN(CPU_ELM_LEN, ncpus);
	tlen += flen;
#ifdef MCE
	tlen += NM_DYNELEMLEN(MCE_ELM_LEN, 1);
#endif
#endif
	info->dynval = malloc(tlen);
	if (!info->dynval){
		err = -1;
		nm_release_cpus(info);
		goto err_xit;
	}

	nm_avnrn_addr = info->dynval + NM_DYNELEMLEN(CPU_ELM_LEN, ncpus) * CPU_IDX_MAX;
#ifdef X86
	nm_freq_addr = nm_avnrn_addr + alen;
#ifdef MCE
	nm_mce_addr = nm_freq_addr + flen;
#endif
#endif
	info->num = ncpus;
	init_cpu_hdrs(info);
	nm_getinfo_cpus(info);

	info->dynlen = tlen;
err_xit:
	free(buf);

	return err;
}


void nm_release_cpus(struct nm_cpu_info_t *info){
	int i;

	if (!info)
		return;

	if (info->prevval){
		for (i=0; i < CPU_IDX_MAX; i++){
			if (info->prevval[i])
				free(info->prevval[i]);
		}
		free(info->prevval);
	}

	if (info->dynval){
		free(info->dynval);
		info->dynval = NULL;
		info->dynlen = 0;
	}

	info->num = 0;
}


int nm_getinfo_cpus(struct nm_cpu_info_t *info){
	FILE *f;
	char *buf;
	uint16_t val;
	/* WARNING!!! */
	char cpustr[32];
	int i, cpu, err;
#ifdef X86
	unsigned int freq;
	char *tmp;
#ifdef MCE
	uint32_t mceval;
#endif
#endif
	uint64_t currvals[CPU_IDX_MAX];
	uint64_t unused;
	time_t diff_t;
	unsigned long avenrun[3];
	unsigned int avenrun_frac[3];

	err = cpu = 0;

	if (!(f = fopen(CPU_FILE, "r"))){
		bzero(info->dynval, info->dynlen);
		init_cpu_hdrs(info);
		return -1;
	}

	if (!(buf = malloc(BUFSIZE))){
		bzero(info->dynval, info->dynlen);
		init_cpu_hdrs(info);
		err = -1;
		goto err_xit;
	}

	diff_t = time(NULL) - info->prev_t;
	if (!diff_t)
		diff_t = 1;

	while (fgets(buf, BUFSIZE, f)){
		if (!strncmp(buf, "cpu", 3) && isdigit(buf[3])){
			sscanf(buf,
#if __WORDSIZE == 64
			    "%s %lu %lu %lu %lu %lu %lu %lu %lu %lu\n",
#else
			    "%s %llu %llu %llu %llu %llu %llu %llu %llu %llu\n",
#endif
			    cpustr,
			    &currvals[CPU_IDX_USR],
			    &currvals[CPU_IDX_NICE],
			    &currvals[CPU_IDX_SYS],
			    &currvals[CPU_IDX_IDLE],
			    &currvals[CPU_IDX_IOW],
			    &currvals[CPU_IDX_IRQ],
			    &currvals[CPU_IDX_SIRQ],
			    &unused,
			    &unused);
			
			for (i=0; i<CPU_IDX_MAX; i++){
				if (info->prevval[i][cpu]){
					val = (currvals[i] - info->prevval[i][cpu])/diff_t;
					NM_VECTADDR(info->dynval, CPU_ELM_LEN, info->num, i, uint16_t)[cpu] =
					    htons(val > 100 ? 100 : val);
				}
				info->prevval[i][cpu] = currvals[i];
			}
			
			if (++cpu >= info->num)
				break;
		}
	}

	info->prev_t += diff_t;

	fclose(f);
#define AVADDR(I)       NM_VECTADDR(nm_avnrn_addr, AVNRN_ELM_LEN, 1, I, uint32_t)
	if ((f = fopen(AVNRN_FILE, "r"))){
		fscanf(f, "%lu.%u %lu.%u %lu.%u",
			&avenrun[0], &avenrun_frac[0],
			&avenrun[1], &avenrun_frac[1],
			&avenrun[2], &avenrun_frac[2]);

		AVADDR(AVNRN_IDX_1M)[0] = htonl((uint32_t)(avenrun[0] * 100 + avenrun_frac[0]));
		AVADDR(AVNRN_IDX_5M)[0] = htonl((uint32_t)(avenrun[1] * 100 + avenrun_frac[1]));
		AVADDR(AVNRN_IDX_15M)[0] = htonl((uint32_t)(avenrun[2] * 100 + avenrun_frac[2]));
	}
#undef AVADDR

#ifdef X86
	fclose(f);
	
	i = 0;
	if ((f = fopen(FREQ_FILE, "r"))){
		while (fgets(buf, BUFSIZE, f)){
			tmp = strstr(buf, "cpu MHz");
			
			if (tmp == buf){
				tmp = strchr(tmp, ':') + 1;
				sscanf(tmp, "%u", &freq);
				
				NM_VECTADDR(nm_freq_addr, CPU_ELM_LEN, info->num, 0, uint16_t)[i++] = 
				    htons(freq);
			}
		}
	}
#ifdef MCE
	fclose(f);

	if (!(f = fopen(MCE_FILE, "r"))){
		NM_VECTADDR(nm_mce_addr, MCE_ELM_LEN, 1, 0, uint32_t)[0] = htonl((uint32_t)NM_CNT_ERR);
	} else {
		i = mceval = 0;
		while (fgets(buf, BUFSIZE, f)){
			tmp = strstr(buf, "MCE:");
			if (tmp == buf){
				tmp += strlen("MCE:");
				
				while ((tmp = strtok((i ? NULL : tmp), " \t")) && 
				    (i < info->num) && isdigit(tmp[0])){
					
					mceval += strtoul(tmp, NULL, 10);
					
					i++;
				}
				
				break;
			}
		}
		NM_VECTADDR(nm_mce_addr, MCE_ELM_LEN, 1, 0, uint32_t)[0] = htonl(NM_GET_CNT32(mceval));
	}
#endif
#endif

	free(buf);

err_xit:
	fclose(f);

	return err;
}


