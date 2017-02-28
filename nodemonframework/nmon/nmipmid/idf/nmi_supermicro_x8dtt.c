#include <ipmi_platform.h>


/* Firmware version 1.37 */


#define NMI_SUPERMICRO_X8DTT_ISENS_CNT	19


struct nmi_sensors_supermicro_x8dtt_t {
	NM_DECLARE_ISENS_RECORD(2, cpu_temp);
	NM_DECLARE_ISENS_RECORD(1, sys_temp);
	NM_DECLARE_ISENS_RECORD(2, vcore);
	NM_DECLARE_ISENS_RECORD(1, v5);
	NM_DECLARE_ISENS_RECORD(1, v12);
	NM_DECLARE_ISENS_RECORD(2, vdimm);
	NM_DECLARE_ISENS_RECORD(1, v1_5);
	NM_DECLARE_ISENS_RECORD(1, v3_3);
	NM_DECLARE_ISENS_RECORD(1, v3_3sb);
	NM_DECLARE_ISENS_RECORD(1, vbat);
	NM_DECLARE_ISENS_RECORD(6, mem_temp);
}__attribute__((packed));


struct nmi_platform_supermicro_x8dtt_t {
	uint16_t	isens_cnt;
	uint16_t	dynlen;
	struct nm_isens_desc_t isens_desc[NMI_SUPERMICRO_X8DTT_ISENS_CNT];
	struct nmi_sensors_supermicro_x8dtt_t tlv;
	uint32_t	crc32;
}__attribute__((packed));


struct nmi_platform_supermicro_x8dtt_t nmi_platform_supermicro_x8dtt = {
	.isens_cnt = NMI_SUPERMICRO_X8DTT_ISENS_CNT,
	.dynlen = sizeof(struct nmi_sensors_supermicro_x8dtt_t),
#define NM_X8DTT_ADDR(n, i)	NM_ISENS_ADDR(nmi_sensors_supermicro_x8dtt_t, n, i)
	.isens_desc = {
		{1, 100, 1, 0, NM_X8DTT_ADDR(cpu_temp, 0)},
		{2, 100, 1, 0, NM_X8DTT_ADDR(cpu_temp, 1)},
		{3, 100, 1, 0, NM_X8DTT_ADDR(sys_temp, 0)},
		{4, 100, 1, 0, NM_X8DTT_ADDR(vcore, 0)},
		{5, 100, 1, 0, NM_X8DTT_ADDR(vcore, 1)},
		{6, 100, 1, 0, NM_X8DTT_ADDR(v5, 0)},
		{7, 100, 1, 0, NM_X8DTT_ADDR(v12, 0)},
		{8, 100, 1, 0, NM_X8DTT_ADDR(vdimm, 0)},
		{9, 100, 1, 0, NM_X8DTT_ADDR(vdimm, 1)},
		{10, 100, 1, 0, NM_X8DTT_ADDR(v1_5, 0)},
		{11, 100, 1, 0, NM_X8DTT_ADDR(v3_3, 0)},
		{12, 100, 1, 0, NM_X8DTT_ADDR(v3_3sb, 0)},
		{13, 100, 1, 0, NM_X8DTT_ADDR(vbat, 0)},
		{32, 100, 1, 0, NM_X8DTT_ADDR(mem_temp, 0)},
		{34, 100, 1, 0, NM_X8DTT_ADDR(mem_temp, 1)},
		{36, 100, 1, 0, NM_X8DTT_ADDR(mem_temp, 2)},
		{38, 100, 1, 0, NM_X8DTT_ADDR(mem_temp, 3)},
		{40, 100, 1, 0, NM_X8DTT_ADDR(mem_temp, 4)},
		{42, 100, 1, 0, NM_X8DTT_ADDR(mem_temp, 5)},
	},
#undef NM_X8DTT_ADDR
	.tlv = {
		NM_ISENS_RECORD(MON_CPU_TEMP, 2, cpu_temp),
		NM_ISENS_RECORD(MON_SYS_TEMP, 1, sys_temp),
		NM_ISENS_RECORD(MON_CPU_VCORE, 2, vcore),
		NM_ISENS_RECORD(MON_V_5, 1, v5),
		NM_ISENS_RECORD(MON_V_12, 1, v12),
		NM_ISENS_RECORD(MON_V_DIMM, 2, vdimm),
		NM_ISENS_RECORD(MON_V_1_5, 1, v1_5),
		NM_ISENS_RECORD(MON_V_3_3, 1, v3_3),
		NM_ISENS_RECORD(MON_V_3_3VSB, 1, v3_3sb),
		NM_ISENS_RECORD(MON_V_BAT, 1, vbat),
		NM_ISENS_RECORD(MON_MEM_TEMP, 6, mem_temp)
	},
};



