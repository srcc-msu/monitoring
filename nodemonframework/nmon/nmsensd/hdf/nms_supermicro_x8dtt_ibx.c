#include <nm_module.h>
#include <nm_platforms.h>


//#define SUPERMICRO_X8DTT_IBX_SENSORS_CNT	17
#define SUPERMICRO_X8DTT_IBX_SENSORS_CNT	13


struct nm_supermicro_x8dtt_ibx_sensors_description_t {
	uint16_t			nmsensd_fw_version;
	uint16_t			sensors_cnt;
	struct nm_sensor_desc_t		sensors[SUPERMICRO_X8DTT_IBX_SENSORS_CNT];
	uint32_t			crc32;
} __attribute__((packed));


struct nm_supermicro_x8dtt_ibx_sensors_description_t platform_description =
{
	.nmsensd_fw_version	= NMSENSD_FW_VERSION,
	.sensors_cnt		= SUPERMICRO_X8DTT_IBX_SENSORS_CNT,
	.sensors = {
		{0,		0,	NM_SENS_TYPE_MGMT_BANK,		0x2F, 0},
		{0,		0x00,	NM_SENS_TYPE_MGMT_WRITE,	0x00, 0},
		{MON_V_1_5,	0,	NM_SENS_TYPE_DISCRETE,		0x12, 0.807},
		{MON_V_3_3,	0,	NM_SENS_TYPE_DISCRETE,		0x1C, 2.477},
		{MON_V_12,	0,	NM_SENS_TYPE_DISCRETE,		0x14, 9.6},
		{MON_V_5,	0,	NM_SENS_TYPE_DISCRETE,		0x13, 4},
		{MON_V_3_3VSB,	0,	NM_SENS_TYPE_DISCRETE,		0x1D, 2.477},
		{MON_V_BAT,	0,	NM_SENS_TYPE_DISCRETE,		0x1E, 2.477},
//		{MON_SYS_FAN,	0,	NM_SENS_TYPE_W83627TACH,	0x2E, 6},
//		{MON_SYS_FAN,	1,	NM_SENS_TYPE_W83627TACH,	0x2F, 6},
//		{MON_SYS_FAN,	2,	NM_SENS_TYPE_W83627TACH,	0x30, 6},
//		{MON_SYS_FAN,	3,	NM_SENS_TYPE_W83627TACH,	0x31, 6},
		{MON_CPU_VCORE,	0,	NM_SENS_TYPE_DISCRETE,		0x10, 0.807},
		{MON_CPU_VCORE,	1,	NM_SENS_TYPE_DISCRETE,		0x11, 0.807},
		{MON_CPU_TEMP,	0,	NM_SENS_TYPE_DISCRETE,		0x26, 100},
		{MON_CPU_TEMP,	1,	NM_SENS_TYPE_DISCRETE,		0x27, 100},
		{MON_SYS_TEMP,	0,	NM_SENS_TYPE_DISCRETE,		0x1F, 100}
	},
	.crc32 = 0
};


