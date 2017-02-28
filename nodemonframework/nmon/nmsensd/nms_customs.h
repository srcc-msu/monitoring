#ifndef _NMS_CUSTOMS_H_
#define _NMS_CUSTOMS_H_

#include <stdint.h>

#include "i2c-dev.h"
#include "nm_platforms.h"


#ifdef NMS_TYAN_5370
static uint16_t nms_t5370_cpu_vcore(int fd, uint8_t indx, float mul){
	int32_t rtmp;
	uint16_t tmp;
	uint16_t val;
	
	if (indx > 1)
		return 0;
	
	if ((rtmp = i2c_smbus_read_byte_data(fd, 0x1B)) == -1)
		return 0;
	
	tmp = rtmp & 0xFFFF;
	
	switch (indx){
	case 0:
		tmp &= 0x03;
		break;
	case 1:
		tmp &= 0x0C;
		tmp >>= 2;
		break;
	default:
		return 0;
	}
	
	if ((rtmp = i2c_smbus_read_byte_data(fd, 0x10 + indx)) == -1)
		return 0;
		
	val = rtmp & 0xFFFF;
		
	return (val * 4 + tmp) * mul;
}


static uint16_t nms_t5370_cpu_temp(int fd, uint8_t indx, float mul){
	int32_t rtmp;
	uint16_t tmp;
	
	if (indx > 1)
		return 0;
	
	if ((rtmp = i2c_smbus_read_byte_data(fd, 0x5E)) == -1)
		return 0;
	
	tmp = rtmp & 0xFFFF;
	
	if ((tmp & 0x30) == 0x30){
		if (!i2c_smbus_read_byte_data(fd, 0x1E))
			tmp = 0x1C + indx;
		else
			tmp = 0x1C + (indx * 2);
	} else {
		tmp = 0x1E - (indx * 2);
	}
	
	rtmp = i2c_smbus_read_byte_data(fd, tmp);
	tmp = rtmp & 0xFFFF;
	
	if (rtmp == -1 || tmp > 0x7F)
		return 0;
	else 
		return tmp * mul;
}


static void nms_t5370_init(uint16_t num, struct nm_platform_internal_t *idxs, struct nm_sensor_desc_t *conf){
	int i;
	
	for (i=0; i < num; i++){
		switch (conf[i].sensor_id){
		case MON_CPU_VCORE:
			idxs[i].read_call = nms_t5370_cpu_vcore;
			break;
		case MON_CPU_TEMP:
			idxs[i].read_call = nms_t5370_cpu_temp;
			break;
		}
	}
}
#endif


#ifdef NMS_TYAN_5382
static uint16_t nms_t5382_vn12(int fd, uint8_t indx, float mul){
	int32_t rtmp;
	uint16_t tmp;
	
	if ((rtmp = i2c_smbus_read_byte_data(fd, 0x25)) == -1)
		return 0;
	
	tmp = rtmp & 0xFFFF;
	
	return tmp * mul - 24;
}


static uint16_t nms_t5382_cpu_temp(int fd, uint8_t indx, float mul){
	int32_t rtmp;
	uint16_t tmp;
	
	if (indx > 1)
		return 0;
	
	if ((rtmp = i2c_smbus_read_byte_data(fd, 0x25 + indx)) == -1)
		return 0;
	
	tmp = rtmp & 0xFFFF;
	
	return tmp + mul;
}


static void nms_t5382_init(uint16_t num, struct nm_platform_internal_t *idxs, struct nm_sensor_desc_t *conf){
	int i;
	
	for (i=0; i < num; i++){
		switch (conf[i].sensor_id){
		case MON_V_N12:
			idxs[i].read_call = nms_t5382_vn12;
			break;
		case MON_CPU_TEMP:
			idxs[i].read_call = nms_t5382_cpu_temp;
			break;
		}
	}
}
#endif


#endif

