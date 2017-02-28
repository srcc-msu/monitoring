#include <ipmi_platform.h>


/* Firmware version 0.a1 */


#define NMI_TPLATFORMS_B20MM_ISENS_CNT	54


struct nmi_sensors_tplatforms_b20mm_t {
	NM_DECLARE_ISENS_RECORD(1, cpu_temp);
	NM_DECLARE_ISENS_RECORD(1, vcore);
	NM_DECLARE_ISENS_RECORD(7, sys_temp);
	NM_DECLARE_ISENS_RECORD(1, v3_3);
	NM_DECLARE_ISENS_RECORD(1, v5);
	NM_DECLARE_ISENS_RECORD(1, v12);
	NM_DECLARE_ISENS_RECORD(10, chas_fan);
	NM_DECLARE_ISENS_RECORD(2, sys_fan);
	NM_DECLARE_ISENS_RECORD(6, psu_temp);
	NM_DECLARE_ISENS_RECORD(12, psu_fan);
	NM_DECLARE_ISENS_RECORD(6, psu_in);
	NM_DECLARE_ISENS_RECORD(6, psu_out);
}__attribute__((packed));


struct nmi_platform_tplatfroms_b20mm_t {
	uint16_t isens_cnt;
	uint16_t dynlen;
	struct nm_isens_desc_t isens_desc[NMI_TPLATFORMS_B20MM_ISENS_CNT];
	struct nmi_sensors_tplatforms_b20mm_t tlv;
	uint32_t crc32;
};


struct nmi_platform_tplatfroms_b20mm_t nmi_platform_tplatfroms_b20mm_t =
{
	.isens_cnt = NMI_TPLATFORMS_B20MM_ISENS_CNT,
	.dynlen = sizeof(struct nmi_sensors_tplatforms_b20mm_t),
#define NM_B20MM_ADDR(n, i)	NM_ISENS_ADDR(nmi_sensors_tplatforms_b20mm_t, n, i)
	.isens_desc = {
		{1, 100 ,1, 0, NM_B20MM_ADDR(cpu_temp, 0)},
		{21, 100 ,1, 0, NM_B20MM_ADDR(vcore, 0)},
		{2, 100 ,1, 0, NM_B20MM_ADDR(sys_temp, 0)},
		{22, 100 ,1, 0, NM_B20MM_ADDR(v3_3, 0)},
		{24, 100 ,1, 0, NM_B20MM_ADDR(v5, 0)},
		{23, 100 ,1, 0, NM_B20MM_ADDR(v12, 0)},
		{100, 1, 10, 0, NM_B20MM_ADDR(chas_fan, 0)},
		{101, 1, 10, 0, NM_B20MM_ADDR(chas_fan, 1)},
		{102, 1, 10, 0, NM_B20MM_ADDR(chas_fan, 2)},
		{103, 1, 10, 0, NM_B20MM_ADDR(chas_fan, 3)},
		{104, 1, 10, 0, NM_B20MM_ADDR(chas_fan, 4)},
		{105, 1, 10, 0, NM_B20MM_ADDR(chas_fan, 5)},
		{106, 1, 10, 0, NM_B20MM_ADDR(chas_fan, 6)},
		{107, 1, 10, 0, NM_B20MM_ADDR(chas_fan, 7)},
		{108, 1, 10, 0, NM_B20MM_ADDR(chas_fan, 8)},
		{109, 1, 10, 0, NM_B20MM_ADDR(chas_fan, 9)},
		{110, 1, 10, 0, NM_B20MM_ADDR(sys_fan, 0)},
		{111, 1, 10, 0, NM_B20MM_ADDR(sys_fan, 1)},
		{9, 100 ,1, 0, NM_B20MM_ADDR(sys_temp, 1)},
		{10, 100 ,1, 0, NM_B20MM_ADDR(sys_temp, 2)},
		{11, 100 ,1, 0, NM_B20MM_ADDR(sys_temp, 3)},
		{12, 100 ,1, 0, NM_B20MM_ADDR(sys_temp, 4)},
		{13, 100 ,1, 0, NM_B20MM_ADDR(sys_temp, 5)},
		{14, 100 ,1, 0, NM_B20MM_ADDR(sys_temp, 6)},
		{3, 100, 1, 0 , NM_B20MM_ADDR(psu_temp, 0)},
		{6, 100, 1, 0 , NM_B20MM_ADDR(psu_temp, 1)},
		{4, 100, 1, 0 , NM_B20MM_ADDR(psu_temp, 2)},
		{7, 100, 1, 0 , NM_B20MM_ADDR(psu_temp, 3)},
		{5, 100, 1, 0 , NM_B20MM_ADDR(psu_temp, 4)},
		{8, 100, 1, 0 , NM_B20MM_ADDR(psu_temp, 5)},
		{112, 1, 10, 0 , NM_B20MM_ADDR(psu_fan, 0)},
		{113, 1, 10, 0 , NM_B20MM_ADDR(psu_fan, 1)},
		{118, 1, 10, 0 , NM_B20MM_ADDR(psu_fan, 2)},
		{119, 1, 10, 0 , NM_B20MM_ADDR(psu_fan, 3)},
		{114, 1, 10, 0 , NM_B20MM_ADDR(psu_fan, 4)},
		{115, 1, 10, 0 , NM_B20MM_ADDR(psu_fan, 5)},
		{120, 1, 10, 0 , NM_B20MM_ADDR(psu_fan, 6)},
		{121, 1, 10, 0 , NM_B20MM_ADDR(psu_fan, 7)},
		{116, 1, 10, 0 , NM_B20MM_ADDR(psu_fan, 8)},
		{117, 1, 10, 0 , NM_B20MM_ADDR(psu_fan, 9)},
		{122, 1, 10, 0 , NM_B20MM_ADDR(psu_fan, 10)},
		{123, 1, 10, 0 , NM_B20MM_ADDR(psu_fan, 11)},
		{26, 10, 1, 0xFFFF, NM_B20MM_ADDR(psu_in, 0)},
		{32, 10, 1, 0xFFFF, NM_B20MM_ADDR(psu_in, 1)},
		{28, 10, 1, 0xFFFF, NM_B20MM_ADDR(psu_in, 2)},
		{34, 10, 1, 0xFFFF, NM_B20MM_ADDR(psu_in, 3)},
		{30, 10, 1, 0xFFFF, NM_B20MM_ADDR(psu_in, 4)},
		{36, 10, 1, 0xFFFF, NM_B20MM_ADDR(psu_in, 5)},
		{25, 100, 1, 0xFFFF, NM_B20MM_ADDR(psu_out, 0)},
		{31, 100, 1, 0xFFFF, NM_B20MM_ADDR(psu_out, 1)},
		{27, 100, 1, 0xFFFF, NM_B20MM_ADDR(psu_out, 2)},
		{33, 100, 1, 0xFFFF, NM_B20MM_ADDR(psu_out, 3)},
		{29, 100, 1, 0xFFFF, NM_B20MM_ADDR(psu_out, 4)},
		{35, 100, 1, 0xFFFF, NM_B20MM_ADDR(psu_out, 5)},
	},
#undef NM_B20MM_ADDR
	.tlv = {
		NM_ISENS_RECORD(MON_CPU_TEMP, 1, cpu_temp),
		NM_ISENS_RECORD(MON_CPU_VCORE, 1, vcore),
		NM_ISENS_RECORD(MON_SYS_TEMP, 7, sys_temp),
		NM_ISENS_RECORD(MON_V_3_3, 1, v3_3),
		NM_ISENS_RECORD(MON_V_5, 1, v5),
		NM_ISENS_RECORD(MON_V_12, 1, v12),
		NM_ISENS_RECORD(MON_CHASSIS_FAN, 10, chas_fan),
		NM_ISENS_RECORD(MON_SYS_FAN, 2, sys_fan),
		NM_ISENS_RECORD(MON_PS_TEMP, 6, psu_temp),
		NM_ISENS_RECORD(MON_PS_FAN, 12, psu_fan),
		NM_ISENS_RECORD(MON_PS_INP_VOLT, 6, psu_in),
		NM_ISENS_RECORD(MON_PS_OUTP_VOLT, 6, psu_out),
	},
};



