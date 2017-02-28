#include <ipmi_platform.h>


/* Firmware version 0.48 */


#define NMI_INTEL_S5520UR_ISENS_CNT	23


struct nmi_sensors_intel_s5520ur_t {
	NM_DECLARE_ISENS_RECORD(1, v1_1);
	NM_DECLARE_ISENS_RECORD(2, vcore);
	NM_DECLARE_ISENS_RECORD(2, vdimm);
	NM_DECLARE_ISENS_RECORD(1, v1_8);
	NM_DECLARE_ISENS_RECORD(1, v3_3);
	NM_DECLARE_ISENS_RECORD(1, v3_3sb);
	NM_DECLARE_ISENS_RECORD(1, vbat);
	NM_DECLARE_ISENS_RECORD(1, v5);
	NM_DECLARE_ISENS_RECORD(1, v5sb);
	NM_DECLARE_ISENS_RECORD(1, v12);
	NM_DECLARE_ISENS_RECORD(1, vn12);
	NM_DECLARE_ISENS_RECORD(1, sys_temp);
	NM_DECLARE_ISENS_RECORD(3, sys_fan);
	NM_DECLARE_ISENS_RECORD(2, ps_in_w);
	NM_DECLARE_ISENS_RECORD(2, ps_out);
	NM_DECLARE_ISENS_RECORD(2, ps_temp);
}__attribute__((packed));


struct nmi_platform_intel_s5520ur_t {
	uint16_t	isens_cnt;
	uint16_t	dynlen;
	struct nm_isens_desc_t isens_desc[NMI_INTEL_S5520UR_ISENS_CNT];
	struct nmi_sensors_intel_s5520ur_t tlv;
	uint32_t	crc32;
}__attribute__((packed));


struct nmi_platform_intel_s5520ur_t nmi_platform_intel_s5520ur = {
	.isens_cnt = NMI_INTEL_S5520UR_ISENS_CNT,
	.dynlen = sizeof(struct nmi_sensors_intel_s5520ur_t),
#define NM_5520_ADDR(n, i)	NM_ISENS_ADDR(nmi_sensors_intel_s5520ur_t, n, i)
	.isens_desc = {
		{16, 100, 1, 0, NM_5520_ADDR(v1_1, 0)},	//1.1V
		{17, 100, 1, 0, NM_5520_ADDR(vcore, 0)},	//Vcc
		{18, 100, 1, 0, NM_5520_ADDR(vcore, 1)},	//Vcc
		{19, 100, 1, 0, NM_5520_ADDR(vdimm, 0)},	//VDimm
		{20, 100, 1, 0, NM_5520_ADDR(vdimm, 1)},	//VDimm
		{21, 100, 1, 0, NM_5520_ADDR(v1_8, 0)},	//1.8V
		{22, 100, 1, 0, NM_5520_ADDR(v3_3, 0)},	//3.3V
		{23, 100, 1, 0, NM_5520_ADDR(v3_3sb, 0)},	//3.3VSB
		{24, 100, 1, 0, NM_5520_ADDR(vbat, 0)},	//VBAT
		{25, 100, 1, 0, NM_5520_ADDR(v5, 0)},	//5V
		{26, 100, 1, 0, NM_5520_ADDR(v5sb, 0)},	//5VSB
		{27, 100, 1, 0, NM_5520_ADDR(v12, 0)},	//12V
		{28, 100, 1, 0, NM_5520_ADDR(vn12, 0)},	//-12V
		{32, 100, 1, 0, NM_5520_ADDR(sys_temp, 0)},//Sys Temp
		{49, 1, 10, 0, NM_5520_ADDR(sys_fan, 0)},//SysFan
		{51, 1, 10, 0, NM_5520_ADDR(sys_fan, 1)},//SysFan
		{56, 1, 10, 0, NM_5520_ADDR(sys_fan, 2)},//SysFan
		{82, 1, 1, 0xFFFF, NM_5520_ADDR(ps_in_w, 0)},//PS in
		{83, 1, 1, 0xFFFF, NM_5520_ADDR(ps_in_w, 1)},//PS in
		{84, 256, 1, 0xFFFF, NM_5520_ADDR(ps_out, 0)},//PS out
		{85, 256, 1, 0xFFFF, NM_5520_ADDR(ps_out, 1)},//PS out
		{84, 100, 1, 0, NM_5520_ADDR(ps_temp, 0)},//PS temp
		{85, 100, 1, 0, NM_5520_ADDR(ps_temp, 1)},//PS temp
	},
#undef NM_5520_ADDR
	.tlv = {
		NM_ISENS_RECORD(MON_V_1_1, 1, v1_1),
		NM_ISENS_RECORD(MON_CPU_VCORE, 2, vcore),
		NM_ISENS_RECORD(MON_V_DIMM, 2, vdimm),
		NM_ISENS_RECORD(MON_V_1_8, 1, v1_8),
		NM_ISENS_RECORD(MON_V_3_3, 1, v3_3),
		NM_ISENS_RECORD(MON_V_3_3VSB, 1, v3_3sb),
		NM_ISENS_RECORD(MON_V_BAT, 1, vbat),
		NM_ISENS_RECORD(MON_V_5, 1, v5),
		NM_ISENS_RECORD(MON_V_5VSB, 1, v5sb),
		NM_ISENS_RECORD(MON_V_12, 1, v12),
		NM_ISENS_RECORD(MON_V_N12, 1, vn12),
		NM_ISENS_RECORD(MON_SYS_TEMP, 1, sys_temp),
		NM_ISENS_RECORD(MON_SYS_FAN, 3, sys_fan),
		NM_ISENS_RECORD(MON_PS_INP_WATTS, 2, ps_in_w),
		NM_ISENS_RECORD(MON_PS_OUTP_LOAD, 2, ps_out),
		NM_ISENS_RECORD(MON_PS_TEMP, 2, ps_temp)
	},
};



