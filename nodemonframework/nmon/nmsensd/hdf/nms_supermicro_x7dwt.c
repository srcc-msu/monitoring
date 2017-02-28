#include <nm_module.h>
#include <nm_platforms.h>


#define SUPERMICRO_X7DWT_SENSORS_CNT	24


struct nm_supermicro_x7dwt_sensors_description_t {
	uint16_t			nmsensd_fw_version;
	uint16_t			sensors_cnt;
	struct nm_sensor_desc_t		sensors[SUPERMICRO_X7DWT_SENSORS_CNT];
	uint32_t			crc32;
} __attribute__((packed));


struct nm_supermicro_x7dwt_sensors_description_t platform_description =
{
	.nmsensd_fw_version	= NMSENSD_FW_VERSION,
	.sensors_cnt		= SUPERMICRO_X7DWT_SENSORS_CNT,
	.sensors = {
		{0,		0,	NM_SENS_TYPE_MGMT_BANK,		0x2D, 0},
		{0,		0x00,	NM_SENS_TYPE_MGMT_WRITE,	0x00, 0},
		{MON_V_1_2,	0,	NM_SENS_TYPE_DISCRETE,		0x12, 0.807},
		{MON_V_1_5,	0,	NM_SENS_TYPE_DISCRETE,		0x15, 1.6},
		{MON_V_3_3,	0,	NM_SENS_TYPE_DISCRETE,		0x16, 1.6},
		{MON_V_12,	0,	NM_SENS_TYPE_DISCRETE,		0x17, 9.6},
		{MON_V_5,	0,	NM_SENS_TYPE_DISCRETE,		0x18, 2.477},
		{MON_V_5VSB,	0,	NM_SENS_TYPE_DISCRETE,		0x19, 2.477},
		{MON_V_BAT,	0,	NM_SENS_TYPE_DISCRETE,		0x1A, 1.6},
		{MON_CPU_FAN,	0,	NM_SENS_TYPE_W83793TACH,	0x2C, 6},
		{MON_CPU_FAN,	1,	NM_SENS_TYPE_W83793TACH,	0x2A, 6},
		{MON_CPU_FAN,	2,	NM_SENS_TYPE_W83793TACH,	0x28, 6},
		{MON_CPU_FAN,	3,	NM_SENS_TYPE_W83793TACH,	0x26, 6},
		{MON_SYS_FAN,	0,	NM_SENS_TYPE_W83793TACH,	0x24, 6},
		{MON_SYS_FAN,	1,	NM_SENS_TYPE_W83793TACH,	0x2E, 6},
		{MON_SYS_FAN,	2,	NM_SENS_TYPE_W83793TACH,	0x30, 6},
		{MON_SYS_FAN,	3,	NM_SENS_TYPE_W83793TACH,	0x32, 6},
		{MON_CPU_VCORE,	0,	NM_SENS_TYPE_DISCRETE,		0x10, 0.807},
		{MON_CPU_VCORE,	1,	NM_SENS_TYPE_DISCRETE,		0x11, 0.807},
		{MON_CPU_TEMP,	0,	NM_SENS_TYPE_DISCRETE,		0x1C, 100},
		{MON_CPU_TEMP,	1,	NM_SENS_TYPE_DISCRETE,		0x1D, 100},
		{MON_CPU_TEMP,	2,	NM_SENS_TYPE_DISCRETE,		0x1E, 100},
		{MON_CPU_TEMP,	3,	NM_SENS_TYPE_DISCRETE,		0x1F, 100},
		{MON_SYS_TEMP,	0,	NM_SENS_TYPE_DISCRETE,		0x20, 100}
	},
	.crc32 = 0
};


