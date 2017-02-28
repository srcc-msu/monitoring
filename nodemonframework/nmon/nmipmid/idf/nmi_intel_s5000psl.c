#include <ipmi_platform.h>


/* Firmware version 0.65 */


#define NMI_INTEL_S5000PSL_ISENS_CNT	14


struct nmi_sensors_intel_s5000psl_t {
	NM_DECLARE_ISENS_RECORD(1, vdimm);
	NM_DECLARE_ISENS_RECORD(3, v1_5);
	NM_DECLARE_ISENS_RECORD(1, v1_8);
	NM_DECLARE_ISENS_RECORD(1, v3_3);
	NM_DECLARE_ISENS_RECORD(1, v3_3sb);
	NM_DECLARE_ISENS_RECORD(1, v5);
	NM_DECLARE_ISENS_RECORD(1, v12);
	NM_DECLARE_ISENS_RECORD(1, sys_temp);
	NM_DECLARE_ISENS_RECORD(2, cpu_fan);
	NM_DECLARE_ISENS_RECORD(2, vcore);
}__attribute__((packed));


struct nmi_platform_intel_s5000psl_t {
	uint16_t	isens_cnt;
	uint16_t	dynlen;
	struct nm_isens_desc_t isens_desc[NMI_INTEL_S5000PSL_ISENS_CNT];
	struct nmi_sensors_intel_s5000psl_t tlv;
	uint32_t	crc32;
}__attribute__((packed));


struct nmi_platform_intel_s5000psl_t nmi_platform_intel_s5000psl = {
	.isens_cnt = NMI_INTEL_S5000PSL_ISENS_CNT,
	.dynlen = sizeof(struct nmi_sensors_intel_s5000psl_t),
#define NM_5000_ADDR(n, i)	NM_ISENS_ADDR(nmi_sensors_intel_s5000psl_t, n, i)
	.isens_desc = {
		{16, 100, 1, 0, NM_5000_ADDR(vdimm, 0)},	//VDimm
		{18, 100, 1, 0, NM_5000_ADDR(v1_5, 0)},	//1.5V
		{19, 100, 1, 0, NM_5000_ADDR(v1_5, 1)},	//1.5V
		{20, 100, 1, 0, NM_5000_ADDR(v1_8, 0)},	//1.8V
		{21, 100, 1, 0, NM_5000_ADDR(v3_3, 0)},	//3.3V
		{22, 100, 1, 0, NM_5000_ADDR(v3_3sb, 0)},	//3.3.sbV
		{23, 100, 1, 0, NM_5000_ADDR(v1_5, 2)},	//1.5V
		{24, 100, 1, 0, NM_5000_ADDR(v5, 0)},	//5V
		{26, 100, 1, 0, NM_5000_ADDR(v12, 0)},	//5V
		{48, 100, 1, 0, NM_5000_ADDR(sys_temp, 0)},//Sys Temp
		{80, 1, 10, 0, NM_5000_ADDR(cpu_fan, 0)},//SysFan
		{81, 1, 10, 0, NM_5000_ADDR(cpu_fan, 1)},//SysFan
		{208, 100, 1, 0, NM_5000_ADDR(vcore, 0)},	//Vcc
		{209, 100, 1, 0, NM_5000_ADDR(vcore, 1)},	//Vcc
	},
#undef NM_5000_ADDR
	.tlv = {
		NM_ISENS_RECORD(MON_V_DIMM, 1, vdimm),
		NM_ISENS_RECORD(MON_V_1_5, 3, v1_5),
		NM_ISENS_RECORD(MON_V_1_8, 1, v1_8),
		NM_ISENS_RECORD(MON_V_3_3, 1, v3_3),
		NM_ISENS_RECORD(MON_V_3_3VSB, 1, v3_3sb),
		NM_ISENS_RECORD(MON_V_5, 1, v5),
		NM_ISENS_RECORD(MON_V_12, 1, v12),
		NM_ISENS_RECORD(MON_SYS_TEMP, 1, sys_temp),
		NM_ISENS_RECORD(MON_CPU_FAN, 2, cpu_fan),
		NM_ISENS_RECORD(MON_CPU_VCORE, 2, vcore),
	},
};



