#ifndef _IPMI_PLATFORM_H_
#define _IPMI_PLATFORM_H_

#include <stddef.h>
#include <stdint.h>

#include <nm_module.h>

#define ISENS_ELM_TYPE		uint16_t
#define ISENS_ELM_LEN		sizeof(ISENS_ELM_TYPE)

struct nm_isens_desc_t {
	uint8_t		ipmi_sensor_num;
	uint16_t	multiplier;
	uint16_t	divisor;
	uint16_t	err_val;
	uint16_t	data_offset;
}__attribute__((packed));


//struct nm_tlv_hdr_t {
//	uint16_t len;
//	uint16_t sensor_id;
//}__attribute__((packed));


#ifndef __BIG_ENDIAN__
#define NM_HTONS(x)	(((x >> 8) & 0x00ff) | ((x << 8) & 0xff00))
#else
#define NM_HTONS(x)	(x)
#endif


#define NM_MT(T, L)		{NM_HTONS(L), NM_HTONS(T)}

#define NM_DECLARE_ISENS_RECORD(C, name) \
    struct nm_tlv_hdr_t name##_hdr; \
    ISENS_ELM_TYPE name[C]


#define NM_ISENS_RECORD(T, C, name) \
    .name##_hdr = NM_MT(T, ISENS_ELM_LEN * C)


#define NM_ISENS_ADDR(sname, name, idx) \
    offsetof(struct sname, name[idx])


#endif


