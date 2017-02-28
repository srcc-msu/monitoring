#include <ipmi_platform.h>


/* Firmware version ? */


#define NMI_FUJITSU_D2860_ISENS_CNT	31


struct nmi_sensors_fujitsu_d2860_t {
	NM_DECLARE_ISENS_RECORD(4, sys_temp);
	NM_DECLARE_ISENS_RECORD(2, cpu_temp);
	NM_DECLARE_ISENS_RECORD(6, mem_temp);
	NM_DECLARE_ISENS_RECORD(1, vbat);
	NM_DECLARE_ISENS_RECORD(2, vcore);
	NM_DECLARE_ISENS_RECORD(2, vdimm);
	NM_DECLARE_ISENS_RECORD(2, v1_1);
	NM_DECLARE_ISENS_RECORD(3, v1_8);
	NM_DECLARE_ISENS_RECORD(3, v12);
	NM_DECLARE_ISENS_RECORD(1, v3_3);
	NM_DECLARE_ISENS_RECORD(1, v3_3sb);
	NM_DECLARE_ISENS_RECORD(1, v5);
	NM_DECLARE_ISENS_RECORD(1, v5sb);
	NM_DECLARE_ISENS_RECORD(1, v1_2);
	NM_DECLARE_ISENS_RECORD(1, v1_5);
}__attribute__((packed));


struct nmi_platform_fujitsu_d2860_t {
	uint16_t	isens_cnt;
	uint16_t	dynlen;
	struct nm_isens_desc_t isens_desc[NMI_FUJITSU_D2860_ISENS_CNT];
	struct nmi_sensors_fujitsu_d2860_t tlv;
	uint32_t	crc32;
}__attribute__((packed));


struct nmi_platform_fujitsu_d2860_t nmi_platform_fujitsu_d2860 = {
	.isens_cnt = NMI_FUJITSU_D2860_ISENS_CNT,
	.dynlen = sizeof(struct nmi_sensors_fujitsu_d2860_t),
#define NM_2860_ADDR(n, i)	NM_ISENS_ADDR(nmi_sensors_fujitsu_d2860_t, n, i)
	.isens_desc = {
		{176, 100, 1, 0, NM_2860_ADDR(sys_temp, 0)},//Sys Temp
		{177, 100, 1, 0, NM_2860_ADDR(cpu_temp, 0)},//Sys Temp
		{178, 100, 1, 0, NM_2860_ADDR(cpu_temp, 1)},//Sys Temp
		{181, 100, 1, 0, NM_2860_ADDR(mem_temp, 0)},	//Mem temp
		{184, 100, 1, 0, NM_2860_ADDR(mem_temp, 1)},	//Mem temp
		{187, 100, 1, 0, NM_2860_ADDR(mem_temp, 2)},	//Mem temp
		{190, 100, 1, 0, NM_2860_ADDR(mem_temp, 3)},	//Mem temp
		{195, 100, 1, 0, NM_2860_ADDR(mem_temp, 4)},	//Mem temp
		{198, 100, 1, 0, NM_2860_ADDR(mem_temp, 5)},	//Mem temp
		{203, 100, 1, 0, NM_2860_ADDR(sys_temp, 1)},//Sys Temp
		{204, 100, 1, 0, NM_2860_ADDR(sys_temp, 2)},//Sys Temp
		{205, 100, 1, 0, NM_2860_ADDR(sys_temp, 3)},//Sys Temp
		{112, 100, 1, 0, NM_2860_ADDR(vbat, 0)},	//VBAT
		{123, 100, 1, 0, NM_2860_ADDR(vcore, 0)},	//Vcc
		{124, 100, 1, 0, NM_2860_ADDR(vcore, 1)},	//Vcc
		{125, 100, 1, 0, NM_2860_ADDR(vdimm, 0)},	//Vcc
		{126, 100, 1, 0, NM_2860_ADDR(vdimm, 1)},	//Vcc
		{127, 100, 1, 0, NM_2860_ADDR(v1_1, 0)},	//1.1V
		{130, 100, 1, 0, NM_2860_ADDR(v1_1, 1)},	//1.1V
		{128, 100, 1, 0, NM_2860_ADDR(v1_8, 0)},	//1.8V
		{129, 100, 1, 0, NM_2860_ADDR(v1_8, 1)},	//1.8V
		{136, 100, 1, 0, NM_2860_ADDR(v1_8, 2)},	//1.8V
		{131, 100, 1, 0, NM_2860_ADDR(v12, 0)},	//12V
		{132, 100, 1, 0, NM_2860_ADDR(v12, 1)},	//12V
		{134, 100, 1, 0, NM_2860_ADDR(v12, 2)},	//12V
		{133, 100, 1, 0, NM_2860_ADDR(v3_3, 0)},	//3.3V
		{122, 100, 1, 0, NM_2860_ADDR(v3_3sb, 0)},	//3.3VSB
		{135, 100, 1, 0, NM_2860_ADDR(v5, 0)},	//5V
		{118, 100, 1, 0, NM_2860_ADDR(v5sb, 0)},	//5VSB
		{138, 100, 1, 0, NM_2860_ADDR(v1_2, 0)},	//1_2V
		{139, 100, 1, 0, NM_2860_ADDR(v1_5, 0)},	//1_5V
	},
#undef NM_2860_ADDR
	.tlv = {
		NM_ISENS_RECORD(MON_SYS_TEMP, 4, sys_temp),
		NM_ISENS_RECORD(MON_CPU_TEMP, 2, cpu_temp),
		NM_ISENS_RECORD(MON_MEM_TEMP, 6, mem_temp),
		NM_ISENS_RECORD(MON_V_BAT, 1, vbat),
		NM_ISENS_RECORD(MON_CPU_VCORE, 2, vcore),
		NM_ISENS_RECORD(MON_V_DIMM, 2, vdimm),
		NM_ISENS_RECORD(MON_V_1_1, 2, v1_1),
		NM_ISENS_RECORD(MON_V_1_8, 3, v1_8),
		NM_ISENS_RECORD(MON_V_12, 3, v12),
		NM_ISENS_RECORD(MON_V_3_3, 1, v3_3),
		NM_ISENS_RECORD(MON_V_3_3VSB, 1, v3_3sb),
		NM_ISENS_RECORD(MON_V_5, 1, v5),
		NM_ISENS_RECORD(MON_V_5VSB, 1, v5sb),
		NM_ISENS_RECORD(MON_V_1_2, 1, v1_2),
		NM_ISENS_RECORD(MON_V_1_5, 1, v1_5),
	},
};



