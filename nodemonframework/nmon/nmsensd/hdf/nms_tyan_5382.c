#include <nm_module.h>
#include <nm_platforms.h>


#define TYAN_5382_SENSORS_CNT	16


struct nm_tyan_5382_sensors_description_t {
	uint16_t			nmsensd_fw_version;
	uint16_t			sensors_cnt;
	struct nm_sensor_desc_t		sensors[TYAN_5382_SENSORS_CNT];
	uint32_t			crc32;
} __attribute__((packed));


struct nm_tyan_5382_sensors_description_t platform_description =
{
	.nmsensd_fw_version	= NMSENSD_FW_VERSION,
	.sensors_cnt		= TYAN_5382_SENSORS_CNT,
	.sensors = {
		{0,		0,	NM_SENS_TYPE_MGMT_BANK,		0x2D, 0},
		{0,		0x4E,	NM_SENS_TYPE_MGMT_WRITE,	0x00, 0},
		{MON_CPU_VCORE,	0,	NM_SENS_TYPE_DISCRETE,		0x20, 0.016},
		{MON_CPU_VCORE,	1,	NM_SENS_TYPE_DISCRETE,		0x21, 0.016},
		{MON_V_3_3,	0,	NM_SENS_TYPE_DISCRETE,		0x22, 0.016},
		{MON_V_5,	0,	NM_SENS_TYPE_DISCRETE,		0x26, 0.024},
		{MON_V_12,	0,	NM_SENS_TYPE_DISCRETE,		0x24, 0.064},
		{MON_V_N12,	0,	/*ignored */ 0,		/*ignored */ 0, 0.1227},
		{0,		0x4E,	NM_SENS_TYPE_MGMT_WRITE,	0x05, 0},
		{MON_V_3_3VSB,	0,	NM_SENS_TYPE_DISCRETE,		0x50, 0.016},
		{MON_V_BAT,	0,	NM_SENS_TYPE_DISCRETE,		0x51, 0.016},
		{0,		0,	NM_SENS_TYPE_MGMT_BANK,		0x2E, 0},
		{MON_CPU_FAN,	0,	NM_SENS_TYPE_SMSCTACH,		0x2A, 60},
		{MON_CPU_FAN,	1,	NM_SENS_TYPE_SMSCTACH,		0x2C, 60},
		{MON_CPU_TEMP,	0,	/*ignored */ 0,		/*ignored */ 0, 90},
		{MON_CPU_TEMP,	1,	/*ignored */ 0,		/*ignored */ 0, 90}
	},
	.crc32 = 0
};


