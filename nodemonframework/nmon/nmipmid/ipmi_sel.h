/*
 * Copyright (c) 2003 Sun Microsystems, Inc.  All Rights Reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * Redistribution of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * Redistribution in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * Neither the name of Sun Microsystems, Inc. or the names of
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * 
 * This software is provided "AS IS," without a warranty of any kind.
 * ALL EXPRESS OR IMPLIED CONDITIONS, REPRESENTATIONS AND WARRANTIES,
 * INCLUDING ANY IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE OR NON-INFRINGEMENT, ARE HEREBY EXCLUDED.
 * SUN MICROSYSTEMS, INC. ("SUN") AND ITS LICENSORS SHALL NOT BE LIABLE
 * FOR ANY DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING
 * OR DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.  IN NO EVENT WILL
 * SUN OR ITS LICENSORS BE LIABLE FOR ANY LOST REVENUE, PROFIT OR DATA,
 * OR FOR DIRECT, INDIRECT, SPECIAL, CONSEQUENTIAL, INCIDENTAL OR
 * PUNITIVE DAMAGES, HOWEVER CAUSED AND REGARDLESS OF THE THEORY OF
 * LIABILITY, ARISING OUT OF THE USE OF OR INABILITY TO USE THIS SOFTWARE,
 * EVEN IF SUN HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */

#ifndef IPMI_SEL_H
#define IPMI_SEL_H

#include <inttypes.h>
#include <ipmi.h>
#include <ipmi_sdr.h>

#define IPMI_CMD_GET_SEL_INFO		0x40
#define IPMI_CMD_GET_SEL_ALLOC_INFO	0x41
#define IPMI_CMD_RESERVE_SEL		0x42
#define IPMI_CMD_GET_SEL_ENTRY		0x43
#define IPMI_CMD_ADD_SEL_ENTRY		0x44
#define IPMI_CMD_PARTIAL_ADD_SEL_ENTRY	0x45
#define IPMI_CMD_DELETE_SEL_ENTRY	0x46
#define IPMI_CMD_CLEAR_SEL		0x47
#define IPMI_CMD_GET_SEL_TIME		0x48
#define IPMI_CMD_SET_SEL_TIME		0x49
#define IPMI_CMD_GET_AUX_LOG_STATUS	0x5A
#define IPMI_CMD_SET_AUX_LOG_STATUS	0x5B

enum {
	IPMI_EVENT_CLASS_DISCRETE,
	IPMI_EVENT_CLASS_DIGITAL,
	IPMI_EVENT_CLASS_THRESHOLD,
	IPMI_EVENT_CLASS_OEM,
};

struct sel_get_rq {
	uint16_t	reserve_id;
	uint16_t	record_id;
	uint8_t	offset;
	uint8_t	length;
} __attribute__ ((packed));

struct standard_spec_sel_rec{
	uint32_t	timestamp;
	uint16_t	gen_id;
	uint8_t	evm_rev;
	uint8_t	sensor_type;
	uint8_t	sensor_num;
#if WORDS_BIGENDIAN
	uint8_t	event_dir  : 1;
	uint8_t	event_type : 7;
#else
	uint8_t	event_type : 7;
	uint8_t	event_dir  : 1;
#endif
#define DATA_BYTE2_SPECIFIED_MASK 0xc0    /* event_data[0] bit mask */
#define DATA_BYTE3_SPECIFIED_MASK 0x30    /* event_data[0] bit mask */
#define EVENT_OFFSET_MASK         0x0f    /* event_data[0] bit mask */
	uint8_t	event_data[3];
};

#define SEL_OEM_TS_DATA_LEN		6
#define SEL_OEM_NOTS_DATA_LEN		13
struct oem_ts_spec_sel_rec{
	uint32_t timestamp;
	uint8_t manf_id[3];
	uint8_t	oem_defined[SEL_OEM_TS_DATA_LEN];
};

struct oem_nots_spec_sel_rec{
	uint8_t oem_defined[SEL_OEM_NOTS_DATA_LEN];
};

struct sel_event_record {
	uint16_t	record_id;
	uint8_t	record_type;
	union{
		struct standard_spec_sel_rec standard_type;
		struct oem_ts_spec_sel_rec oem_ts_type;
		struct oem_nots_spec_sel_rec oem_nots_type;
	} sel_type;
} __attribute__ ((packed));

struct ipmi_event_sensor_types {
	uint8_t	code;
	uint8_t	offset;
#define ALL_OFFSETS_SPECIFIED  0xff
	uint8_t   data;
	uint8_t	class;
	const char	* type;
	const char	* desc;
};


int ipmi_sel_main(struct ipmi_intf *, int, char **);
void ipmi_sel_print_std_entry(struct ipmi_intf * intf, struct sel_event_record * evt);
void ipmi_sel_print_std_entry_verbose(struct ipmi_intf * intf, struct sel_event_record * evt);
void ipmi_sel_print_extended_entry(struct ipmi_intf * intf, struct sel_event_record * evt);
void ipmi_sel_print_extended_entry_verbose(struct ipmi_intf * intf, struct sel_event_record * evt);
void ipmi_get_event_desc(struct ipmi_intf * intf, struct sel_event_record * rec, char ** desc);
const char * ipmi_sel_get_sensor_type(uint8_t code);
const char * ipmi_sel_get_sensor_type_offset(uint8_t code, uint8_t offset);
uint16_t ipmi_sel_get_std_entry(struct ipmi_intf * intf, uint16_t id, struct sel_event_record * evt);
char * get_newisys_evt_desc(struct ipmi_intf * intf, struct sel_event_record * rec);
IPMI_OEM ipmi_get_oem(struct ipmi_intf * intf);
char * ipmi_get_oem_desc(struct ipmi_intf * intf, struct sel_event_record * rec);
int ipmi_sel_oem_init(const char * filename);

#endif /* IPMI_SEL_H */
