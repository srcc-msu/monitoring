#include <nm_module.h>
#include <nm_platforms.h>


#define TYAN_5370_SENSORS_CNT	21


struct nm_tyan_5370_sensors_description_t {
	uint16_t			nmsensd_fw_version;
	uint16_t			sensors_cnt;
	struct nm_sensor_desc_t		sensors[TYAN_5370_SENSORS_CNT];
	uint32_t			crc32;
} __attribute__((packed));


struct nm_tyan_5370_sensors_description_t platform_description =
{
	.nmsensd_fw_version	= NMSENSD_FW_VERSION,
	.sensors_cnt		= TYAN_5370_SENSORS_CNT,
	.sensors = {
		{0,		0,	NM_SENS_TYPE_MGMT_BANK,		0x2D, 0},
		{MON_SYS_FAN,	0,	NM_SENS_TYPE_SMSCTACH,		0x28, 6},
		{MON_SYS_FAN,	2,	NM_SENS_TYPE_SMSCTACH,		0x2A, 6},
		{MON_SYS_FAN,	3,	NM_SENS_TYPE_SMSCTACH,		0x2E, 6},
		{MON_V_3_3,	0,	NM_SENS_TYPE_DISCRETE,		0x22, 1.71},
		{MON_V_5,	0,	NM_SENS_TYPE_DISCRETE,		0x23, 2.6},
		{MON_V_12,	0,	NM_SENS_TYPE_DISCRETE,		0x24, 6.3},
		{MON_V_3_3VSB,	0,	NM_SENS_TYPE_DISCRETE,		0x99, 1.71},
		{MON_V_5VSB,	0,	NM_SENS_TYPE_DISCRETE,		0x20, 2.6},
		{MON_V_BAT,	0,	NM_SENS_TYPE_DISCRETE,		0x9A, 1.71},
		{MON_SYS_TEMP,	0,	NM_SENS_TYPE_DISCRETE,		0x27, 100},
		{0,		0,	NM_SENS_TYPE_MGMT_BANK,		0x7F, 0},
		{0,		0x4E,	NM_SENS_TYPE_MGMT_WRITE,	0x00, 0},
		{MON_CPU_FAN,	0,	NM_SENS_TYPE_W83793TACH,	0x2C, 6},
		{MON_CPU_FAN,	1,	NM_SENS_TYPE_W83793TACH,	0x2A, 6},
		{MON_CPU_FAN,	2,	NM_SENS_TYPE_W83793TACH,	0x28, 6},
		{MON_SYS_FAN,	1,	NM_SENS_TYPE_W83793TACH,	0x2E, 6},
		{MON_CPU_VCORE,	0,	/*ignored */ 0,		/*ignored */ 0, 0.2},
		{MON_CPU_VCORE,	1,	/*ignored */ 0,		/*ignored */ 0, 0.2},
		{MON_CPU_TEMP,	0,	/*ignored */ 0,		/*ignored */ 0, 100},
		{MON_CPU_TEMP,	1,	/*ignored */ 0,		/*ignored */ 0, 100}
	},
	.crc32 = 0
};


