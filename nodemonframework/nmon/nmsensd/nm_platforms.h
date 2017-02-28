#ifndef _NM_PLATFORMS_H_
#define _NM_PLATFORMS_H_

#include <stdint.h>


#define NMSENSD_FW_VERSION	1


#define NM_SENS_TYPE_DISCRETE		0
#define NM_SENS_TYPE_SMSCTACH		1
#define NM_SENS_TYPE_W83793TACH		2
#define NM_SENS_TYPE_W83627TACH		3

#define NM_SENS_TYPE_MGMT_WRITE		4
#define NM_SENS_TYPE_MGMT_BANK		5


static inline int nm_sens_is_mgmt(uint8_t sens){
	return (sens == NM_SENS_TYPE_MGMT_WRITE ||
		sens == NM_SENS_TYPE_MGMT_BANK);
}


struct nm_sensor_desc_t {
	uint16_t	sensor_id;		/* 0 in mgmt record */
	uint8_t		sensor_index;		
	uint8_t		sensor_type;
	uint8_t		sensor_address;		/* value in mgmt_write_mode */
	float		sensor_divisor;		/* 0 in mgmt record */
}__attribute__((packed));


typedef uint16_t (*nm_custom_sens_t)(int, uint8_t, float);


struct nm_platform_internal_t {
	uint16_t		*addr;
	nm_custom_sens_t	read_call;
};


struct nm_platform_desc_t {
	const char		*vendor_name;
	const char		*product_name;
	const char		*firmware_name;
	void (*init_call)(uint16_t, struct nm_platform_internal_t *, struct nm_sensor_desc_t *);
};


struct nm_sensor_cnt_t {
	uint16_t	mon_type;
	uint8_t		count;
};


/*
 * USAGE: 
 * Your code should be look like this (example):
 * -----------------8<-------------------
 *
 * #define PLAFORM_SENSORS_CNT	<number_of_sensors>
 *
 * struct nm_platform_sensors_description_t {
 * 	uint16_t			nmsensd_fw_version;
 * 	uint16_t			sensors_cnt;
 * 	struct nm_sensor_desc_t		sensors[PLATFORM_SENSORS_CNT];
 * 	uint32_t			crc32;
 * }__attribute__((packed));
 *
 * struct nm_platform_description_t platform_description = {
 * 	.sensors_cnt = PLATFORM_SENSORS_CNT,
 * 	.nmsensd_fw_version = NMSENSD_FW_VERSION,
 * 	.sensors = {
 * 		{MON_SYS_FAN, 0, NM_SENS_TYPE_SMSCTACH, 0x28, 6},
 * 		{MON_SYS_FAN, 2, NM_SENS_TYPE_SMSCTACH, 0x2A, 6},
 * 		...
 * 		{MON_V_BAT, 0, NM_SENS_TYPE_DISCRETE, 0x9A, 1.71}
 * 	},
 * 	.crc32 = 0x00000000	//Should be computed later
 * };
 *
 * -----------------8<-------------------
 *
 * Source must be compiled into .o (object file) and extracted with ld.
 * Then external program should compute crc32 checksum and overwite last 4 bytes in file.
 *
 */


#endif

