#ifndef _CHECKCONF_H_
#define _CHECKCONF_H_

#ifdef __cplusplus
extern "C" {
#endif


struct cpu_info {
	unsigned ci_amount;
	unsigned ci_family;
	char *ci_vendor;
	char *ci_model;
	unsigned ci_speed;
	unsigned ci_clock;
};


struct mem_info {
	unsigned mi_amount_mb;
	unsigned mi_type;
	char **mi_banks;
};


struct mb_info {
	char *mi_vendor;
	char *mi_product;
	char *mi_version;
	char *mi_id;
};


struct smbios_info {
	char *si_vendor;
	char *si_version;
	char *si_reldate;
	char *si_rev;
};


struct vendbios_info {
	char *vi_vendor;
	char *vi_type;
	char *vi_reldate;
	char *vi_rev;
};


struct ipmi_info {
	char *ii_type;
	char *ii_specver;
};


struct sys_info {
	struct mb_info 		si_mb;
#ifndef TINYSYSNFO
	struct cpu_info 	si_cpu;
	struct mem_info 	si_mem;
	struct smbios_info 	si_smbios;
	struct vendbios_info	si_vendbios;
	struct ipmi_info 	si_ipmi;
	char 			**si_ib;
	char 			**si_eth;
	char 			**si_hd;
#endif
};


struct sys_info *init_sys_info(void);
void free_sys_info(struct sys_info*);


#ifdef __cplusplus
}
#endif


#endif

