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

#include <string.h>

//#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>

#include <ipmi.h>
#include <ipmi_mc.h>
#include <ipmi_sdr.h>
#include <ipmi_intf.h>
#include <ipmi_sel.h>
#include <ipmi_entity.h>
#include <ipmi_constants.h>

#if HAVE_CONFIG_H
# include <config.h>
#endif

static int use_built_in;	/* Uses DeviceSDRs instead of SDRR */
static int sdr_max_read_len = GET_SDR_ENTIRE_RECORD;
//static int sdr_extended = 0;
static long sdriana = 0;

static struct sdr_record_list *sdr_list_head = NULL;
static struct sdr_record_list *sdr_list_tail = NULL;
static struct ipmi_sdr_iterator *sdr_list_itr = NULL;


#include <ipmi_platform.h>
//TODO: TO BE CHANGED!!!!!!!
extern uint16_t isens_cnt;
extern uint16_t dynlen;
extern struct nm_isens_desc_t *isens_desc;
extern void *tlvdata;


static int nmi_lookup_isens(uint8_t sensor_num){
	uint16_t i;
	
	for (i = 0; i < isens_cnt; i++)
		if (sensor_num == isens_desc[i].ipmi_sensor_num)
			return i;
	
	return -1;
}

double nmi_pow(double base, int pow){
	int i;
	double res;
	
	if (base == 0.0)
		return 0.0;
	
	if (pow > 0){
		res = base;
		
		for (i = 1; i < pow; i++)
			res *= base;
	} else if (pow < 0){
		res = 1;
		for (i = -1; i >= pow; i--)
			res /= base;
	} else res = 1.0;
	
	return res;
}


/* sdr_convert_sensor_reading  -  convert raw sensor reading
 *
 * @sensor:	sensor record
 * @val:	raw sensor reading
 *
 * returns floating-point sensor reading
 */
double
sdr_convert_sensor_reading(struct sdr_record_full_sensor *sensor, uint8_t val)
{
	int m, b, k1, k2;
	double result = 0.0;

	m = __TO_M(sensor->mtol);
	b = __TO_B(sensor->bacc);
	k1 = __TO_B_EXP(sensor->bacc);
	k2 = __TO_R_EXP(sensor->bacc);

	switch (sensor->unit.analog) {
	case 0:
		result = (double) (((m * val) +
				    (b * nmi_pow(10, k1))) * nmi_pow(10, k2));
		break;
	case 1:
		if (val & 0x80)
			val++;
		/* Deliberately fall through to case 2. */
	case 2:
		result = (double) (((m * (int8_t) val) +
				    (b * nmi_pow(10, k1))) * nmi_pow(10, k2));
		break;
	default:
		/* Oops! This isn't an analog sensor. */
		return 0.0;
	}

	return result;
}


/* ipmi_sdr_get_sensor_reading  -  retrieve a raw sensor reading
 *
 * @intf:	ipmi interface
 * @sensor:	sensor id
 *
 * returns ipmi response structure
 */
struct ipmi_rs *
ipmi_sdr_get_sensor_reading(struct ipmi_intf *intf, uint8_t sensor)
{
	struct ipmi_rq req;

	memset(&req, 0, sizeof (req));
	req.msg.netfn = IPMI_NETFN_SE;
	req.msg.cmd = GET_SENSOR_READING;
	req.msg.data = &sensor;
	req.msg.data_len = 1;

	return intf->sendrecv(intf, &req);
}


static inline int ipmi_sdr_get_status(struct sdr_record_full_sensor *sensor, uint8_t stat){
	return (stat & 
	    (SDR_SENSOR_STAT_LO_NR | SDR_SENSOR_STAT_HI_NR | 
	    SDR_SENSOR_STAT_LO_NC | SDR_SENSOR_STAT_HI_NC)) ? 
	    0 : 1;
}



/* ipmi_sdr_get_header  -  retreive SDR record header
 *
 * @intf:	ipmi interface
 * @itr:	sdr iterator
 *
 * returns pointer to static sensor retrieval struct
 * returns NULL on error
 */
static struct sdr_get_rs *
ipmi_sdr_get_header(struct ipmi_intf *intf, struct ipmi_sdr_iterator *itr)
{
	struct ipmi_rq req;
	struct ipmi_rs *rsp;
	struct sdr_get_rq sdr_rq;
	static struct sdr_get_rs sdr_rs;
	int try = 0;

	memset(&sdr_rq, 0, sizeof (sdr_rq));
	sdr_rq.reserve_id = itr->reservation;
	sdr_rq.id = itr->next;
	sdr_rq.offset = 0;
	sdr_rq.length = 5;	/* only get the header */

	memset(&req, 0, sizeof (req));
	if (use_built_in == 0) {
		req.msg.netfn = IPMI_NETFN_STORAGE;
		req.msg.cmd = GET_SDR;
	} else {
		req.msg.netfn = IPMI_NETFN_SE;
		req.msg.cmd = GET_DEVICE_SDR;
	}
	req.msg.data = (uint8_t *) & sdr_rq;
	req.msg.data_len = sizeof (sdr_rq);

	for (try = 0; try < 5; try++) {
		sdr_rq.reserve_id = itr->reservation;
		rsp = intf->sendrecv(intf, &req);
		if (rsp == NULL) {
			continue;
		} else if (rsp->ccode == 0xc5) {
			/* lost reservation */

			sleep(rand() & 3);

			if (ipmi_sdr_get_reservation(intf, &(itr->reservation))
			    < 0) {
				return NULL;
			}
		} else if (rsp->ccode > 0) {
			continue;
		} else {
			break;
		}
	}

	if (!rsp)
		return NULL;

	memcpy(&sdr_rs, rsp->data, sizeof (sdr_rs));

	if (sdr_rs.length == 0) {
		return NULL;
	}

	/* achu (chu11 at llnl dot gov): - Some boards are stupid and
	 * return a record id from the Get SDR Record command
	 * different than the record id passed in.  If we find this
	 * situation, we cheat and put the original record id back in.
	 * Otherwise, a later Get SDR Record command will fail with
	 * completion code CBh = "Requested Sensor, data, or record
	 * not present"
	 */
	if (sdr_rs.id != itr->next) {
		sdr_rs.id = itr->next;
	}

	return &sdr_rs;
}

/* ipmi_sdr_get_next_header  -  retreive next SDR header
 *
 * @intf:	ipmi interface
 * @itr:	sdr iterator
 *
 * returns pointer to sensor retrieval struct
 * returns NULL on error
 */
struct sdr_get_rs *
ipmi_sdr_get_next_header(struct ipmi_intf *intf, struct ipmi_sdr_iterator *itr)
{
	struct sdr_get_rs *header;

	if (itr->next == 0xffff)
		return NULL;

	header = ipmi_sdr_get_header(intf, itr);
	if (header == NULL)
		return NULL;

	itr->next = header->next;

	return header;
}

/* helper macro for printing CSV output */
#define SENSOR_PRINT_CSV(FLAG, READ)				\
	if (FLAG)						\
		printf("%.3f,",					\
		       sdr_convert_sensor_reading(		\
			       sensor, READ));			\
	else							\
		printf(",");

/* helper macro for priting analog values */
#define SENSOR_PRINT_NORMAL(NAME, READ)				\
	if (sensor->analog_flag.READ != 0) {			\
		printf(" %-21s : ", NAME);			\
		printf("%.3f\n", sdr_convert_sensor_reading(	\
			         sensor, sensor->READ));	\
	}

/* helper macro for printing sensor thresholds */
#define SENSOR_PRINT_THRESH(NAME, READ, FLAG)			\
	if (sensor->sensor.init.thresholds &&			\
	    sensor->mask.type.threshold.read.FLAG != 0) {	\
		printf(" %-21s : ", NAME);			\
		printf("%.3f\n", sdr_convert_sensor_reading(	\
			     sensor, sensor->threshold.READ));	\
	}

/* ipmi_sdr_print_sensor_full  -  print full SDR record
 *
 * @intf:	ipmi interface
 * @sensor:	full sensor structure
 *
 * returns 0 on success
 * returns -1 on error
 */
int
ipmi_sdr_print_sensor_full(struct ipmi_intf *intf,
			   struct sdr_record_full_sensor *sensor, uint16_t *ssmc)
{
	int validread = 1;
	int indx;
	double val = 0.0; //, creading = 0.0;
	struct ipmi_rs *rsp;

	if (sensor == NULL)
		return -1;

	/* only handle linear sensors and linearized sensors (for now) */
	if (sensor->linearization >= SDR_SENSOR_L_NONLINEAR) {
		return -1;
	}

	// Handle only sensors in config
	indx = nmi_lookup_isens(sensor->keys.sensor_num);
	if (indx == -1)
		return 0;
	// Increment state machine counter
	(*ssmc)++;
	
	/* get sensor reading */
	rsp = ipmi_sdr_get_sensor_reading(intf, sensor->keys.sensor_num);

	if (rsp == NULL) {
		validread = 0;
	}
	else if (rsp->ccode > 0) {
		validread = 0;
	} else {
		if (IS_READING_UNAVAILABLE(rsp->data[1])) {
			/* sensor reading unavailable */
			validread = 0;
		} else if (IS_SCANNING_DISABLED(rsp->data[1])) {
			/* Sensor Scanning Disabled */
			validread = 0;
		} else if (rsp->data[0] != 0) {
			/* convert RAW reading into units */
			val = sdr_convert_sensor_reading(sensor, rsp->data[0]);
		}
	}

	if (validread && ipmi_sdr_get_status(sensor, rsp->data[2]))
		*(uint16_t *)(tlvdata + isens_desc[indx].data_offset) =
		    htons(val * isens_desc[indx].multiplier / isens_desc[indx].divisor);
	else 
		*(uint16_t *)(tlvdata + isens_desc[indx].data_offset) =
		    htons(isens_desc[indx].err_val);

	return 0;	/* done */
}


/* ipmi_sdr_print_rawentry  -  Print SDR entry from raw data
 *
 * @intf:	ipmi interface
 * @type:	sensor type
 * @raw:	raw sensor data
 * @len:	length of raw sensor data
 *
 * returns 0 on success
 * returns -1 on error
 */
int
ipmi_sdr_print_rawentry(struct ipmi_intf *intf, uint8_t type,
			uint8_t * raw, int len, uint16_t *ssmc)
{
	int rc = 0;

	if (type == SDR_RECORD_TYPE_FULL_SENSOR)
		rc = ipmi_sdr_print_sensor_full(intf,
						(struct sdr_record_full_sensor
						 *) raw, ssmc);
	return rc;
}

/* ipmi_sdr_print_listentry  -  Print SDR entry from list
 *
 * @intf:	ipmi interface
 * @entry:	sdr record list entry
 *
 * returns 0 on success
 * returns -1 on error
 */
int
ipmi_sdr_print_listentry(struct ipmi_intf *intf, struct sdr_record_list *entry, uint16_t *ssmc)
{
	int rc = 0;
	
	if (entry->type == SDR_RECORD_TYPE_FULL_SENSOR)
		rc = ipmi_sdr_print_sensor_full(intf, entry->record.full, ssmc);

	return rc;
}

/* ipmi_sdr_print_sdr  -  iterate through SDR printing records
 *
 * intf:	ipmi interface
 * type:	record type to print
 *
 * returns 0 on success
 * returns -1 on error
 */
int
ipmi_sdr_print_sdr(struct ipmi_intf *intf, uint8_t type)
{
	uint16_t ssmc = 0; // Sensor State Machine Counter
	struct sdr_get_rs *header;
	struct sdr_record_list *e;
	int rc = 0;


	if (sdr_list_itr == NULL) {
		sdr_list_itr = ipmi_sdr_start(intf);
		if (sdr_list_itr == NULL) {
			return -1;
		}
	}

	for (e = sdr_list_head; e != NULL; e = e->next) {
		if (type != e->type && type != 0xff && type != 0xfe)
			continue;
		if (type == 0xfe &&
		    e->type != SDR_RECORD_TYPE_FULL_SENSOR &&
		    e->type != SDR_RECORD_TYPE_COMPACT_SENSOR)
			continue;
		if (ipmi_sdr_print_listentry(intf, e, &ssmc) < 0)
			rc = -1;
		if (ssmc >= isens_cnt)
			return rc;
	}

	while (((header = ipmi_sdr_get_next_header(intf, sdr_list_itr)) != NULL) && ssmc < isens_cnt) {
		uint8_t *rec;
		struct sdr_record_list *sdrr;

		rec = ipmi_sdr_get_record(intf, header, sdr_list_itr);
		if (rec == NULL) {
			rc = -1;
			continue;
		}

		sdrr = malloc(sizeof (struct sdr_record_list));
		if (sdrr == NULL) {
			break;
		}
		memset(sdrr, 0, sizeof (struct sdr_record_list));
		sdrr->id = header->id;
		sdrr->type = header->type;

		if (header->type == SDR_RECORD_TYPE_FULL_SENSOR){
			sdrr->record.full =
			    (struct sdr_record_full_sensor *) rec;
		} else {
			free(rec);
			continue;
		}

		if (type == header->type || type == 0xff ||
		    (type == 0xfe &&
		     (header->type == SDR_RECORD_TYPE_FULL_SENSOR ||
		      header->type == SDR_RECORD_TYPE_COMPACT_SENSOR))) {
			if (ipmi_sdr_print_rawentry(intf, header->type,
						    rec, header->length, &ssmc) < 0)
				rc = -1;
		}

		/* add to global record liset */
		if (sdr_list_head == NULL)
			sdr_list_head = sdrr;
		else
			sdr_list_tail->next = sdrr;

		sdr_list_tail = sdrr;
	}

	return rc;
}

/* ipmi_sdr_get_reservation  -  Obtain SDR reservation ID
 *
 * @intf:	ipmi interface
 * @reserve_id:	pointer to short int for storing the id
 *
 * returns 0 on success
 * returns -1 on error
 */
int
ipmi_sdr_get_reservation(struct ipmi_intf *intf, uint16_t * reserve_id)
{
	struct ipmi_rs *rsp;
	struct ipmi_rq req;

	/* obtain reservation ID */
	memset(&req, 0, sizeof (req));

	if (use_built_in == 0) {
		req.msg.netfn = IPMI_NETFN_STORAGE;
	} else {
		req.msg.netfn = IPMI_NETFN_SE;
	}

	req.msg.cmd = GET_SDR_RESERVE_REPO;
	rsp = intf->sendrecv(intf, &req);

	/* be slient for errors, they are handled by calling function */
	if (rsp == NULL)
		return -1;
	if (rsp->ccode > 0)
		return -1;

	*reserve_id = ((struct sdr_reserve_repo_rs *) &(rsp->data))->reserve_id;

	return 0;
}

/* ipmi_sdr_start  -  setup sdr iterator
 *
 * @intf:	ipmi interface
 *
 * returns sdr iterator structure pointer
 * returns NULL on error
 */
struct ipmi_sdr_iterator *
ipmi_sdr_start(struct ipmi_intf *intf)
{
	struct ipmi_sdr_iterator *itr;
	struct ipmi_rs *rsp;
	struct ipmi_rq req;

	struct ipm_devid_rsp *devid;

	itr = malloc(sizeof (struct ipmi_sdr_iterator));
	if (itr == NULL) {
		return NULL;
	}

	/* check SDRR capability */
	memset(&req, 0, sizeof (req));
	req.msg.netfn = IPMI_NETFN_APP;
	req.msg.cmd = BMC_GET_DEVICE_ID;
	req.msg.data_len = 0;

	rsp = intf->sendrecv(intf, &req);

	if (rsp == NULL) {
		free(itr);
		return NULL;
	}
	devid = (struct ipm_devid_rsp *) rsp->data;

   sdriana =  (long)IPM_DEV_MANUFACTURER_ID(devid->manufacturer_id);

	if (devid->device_revision & IPM_DEV_DEVICE_ID_SDR_MASK) {
		if ((devid->adtl_device_support & 0x02) == 0) {
			if ((devid->adtl_device_support & 0x01)) {
				use_built_in = 1;
			} else {
				free(itr);
				return NULL;
			}
		}
	}
   /***********************/
	if (use_built_in == 0) {
		struct sdr_repo_info_rs sdr_info;
		/* get sdr repository info */
		memset(&req, 0, sizeof (req));
		req.msg.netfn = IPMI_NETFN_STORAGE;
		req.msg.cmd = GET_SDR_REPO_INFO;

		rsp = intf->sendrecv(intf, &req);
		if (rsp == NULL) {
			free(itr);
			return NULL;
		}
		if (rsp->ccode > 0) {
			free(itr);
			return NULL;
		}

		memcpy(&sdr_info, rsp->data, sizeof (sdr_info));
		/* IPMIv1.0 == 0x01
		   * IPMIv1.5 == 0x51
		   * IPMIv2.0 == 0x02
		 */
		if ((sdr_info.version != 0x51) &&
		    (sdr_info.version != 0x01) &&
		    (sdr_info.version != 0x02)) {
		}

		itr->total = sdr_info.count;
		itr->next = 0;

	} else {
		struct sdr_device_info_rs sdr_info;
		/* get device sdr info */
		memset(&req, 0, sizeof (req));
		req.msg.netfn = IPMI_NETFN_SE;
		req.msg.cmd = GET_DEVICE_SDR_INFO;

		rsp = intf->sendrecv(intf, &req);
		if (!rsp || !rsp->data_len || rsp->ccode) {
			printf("Err in cmd get sensor sdr info\n");
			free(itr);
			return NULL;
		}
		memcpy(&sdr_info, rsp->data, sizeof (sdr_info));

		itr->total = sdr_info.count;
		itr->next = 0;
	}

	if (ipmi_sdr_get_reservation(intf, &(itr->reservation)) < 0) {
		free(itr);
		return NULL;
	}

	return itr;
}

/* ipmi_sdr_get_record  -  return RAW SDR record
 *
 * @intf:	ipmi interface
 * @header:	SDR header
 * @itr:	SDR iterator
 *
 * returns raw SDR data
 * returns NULL on error
 */
uint8_t *
ipmi_sdr_get_record(struct ipmi_intf * intf, struct sdr_get_rs * header,
		    struct ipmi_sdr_iterator * itr)
{
	struct ipmi_rq req;
	struct ipmi_rs *rsp;
	struct sdr_get_rq sdr_rq;
	uint8_t *data;
	int i = 0, len = header->length;

	if (len < 1)
		return NULL;

	data = malloc(len + 1);
	if (data == NULL) {
		return NULL;
	}
	memset(data, 0, len + 1);

	memset(&sdr_rq, 0, sizeof (sdr_rq));
	sdr_rq.reserve_id = itr->reservation;
	sdr_rq.id = header->id;
	sdr_rq.offset = 0;

	memset(&req, 0, sizeof (req));
	if (use_built_in == 0) {
		req.msg.netfn = IPMI_NETFN_STORAGE;
		req.msg.cmd = GET_SDR;
	} else {
		req.msg.netfn = IPMI_NETFN_SE;
		req.msg.cmd = GET_DEVICE_SDR;
	}
	req.msg.data = (uint8_t *) & sdr_rq;
	req.msg.data_len = sizeof (sdr_rq);

	/* read SDR record with partial reads
	 * because a full read usually exceeds the maximum
	 * transport buffer size.  (completion code 0xca)
	 */
	while (i < len) {
		sdr_rq.length = (len - i < sdr_max_read_len) ?
		    len - i : sdr_max_read_len;
		sdr_rq.offset = i + 5;	/* 5 header bytes */

		rsp = intf->sendrecv(intf, &req);
		if (rsp == NULL) {
			free(data);
			return NULL;
		}

		switch (rsp->ccode) {
		case 0xca:
			/* read too many bytes at once */
			sdr_max_read_len = (sdr_max_read_len >> 1) - 1;
			continue;
		case 0xc5:
			/* lost reservation */

			sleep(rand() & 3);

			if (ipmi_sdr_get_reservation(intf, &(itr->reservation))
			    < 0) {
				free(data);
				return NULL;
			}
			sdr_rq.reserve_id = itr->reservation;
			continue;
		}

		/* special completion codes handled above */
		if (rsp->ccode > 0 || rsp->data_len == 0) {
			free(data);
			return NULL;
		}

		memcpy(data + i, rsp->data + 2, sdr_rq.length);
		i += sdr_max_read_len;
	}

	return data;
}

/* ipmi_sdr_end  -  cleanup SDR iterator
 *
 * @intf:	ipmi interface
 * @itr:	SDR iterator
 *
 * no meaningful return code
 */
void
ipmi_sdr_end(struct ipmi_intf *intf, struct ipmi_sdr_iterator *itr)
{
	if (itr)
		free(itr);
}

/* __sdr_list_add  -  helper function to add SDR record to list
 *
 * @head:	list head
 * @entry:	new entry to add to end of list
 *
 * returns 0 on success
 * returns -1 on error
 */
/*
static int
__sdr_list_add(struct sdr_record_list *head, struct sdr_record_list *entry)
{
	struct sdr_record_list *e;
	struct sdr_record_list *new;

	if (head == NULL)
		return -1;

	new = malloc(sizeof (struct sdr_record_list));
	if (new == NULL) {
		return -1;
	}
	memcpy(new, entry, sizeof (struct sdr_record_list));

	e = head;
	while (e->next)
		e = e->next;
	e->next = new;
	new->next = NULL;

	return 0;
}
*/

/* __sdr_list_empty  -  low-level handler to clean up record list
 *
 * @head:	list head to clean
 *
 * no meaningful return code
 */
/*
static void
__sdr_list_empty(struct sdr_record_list *head)
{
	struct sdr_record_list *e, *f;
	for (e = head; e != NULL; e = f) {
		f = e->next;
		free(e);
	}
	head = NULL;
}
*/

/* ipmi_sdr_list_empty  -  clean global SDR list
 *
 * @intf:	ipmi interface
 *
 * no meaningful return code
 */
void
ipmi_sdr_list_empty(struct ipmi_intf *intf)
{
	struct sdr_record_list *list, *next;

	ipmi_sdr_end(intf, sdr_list_itr);

	for (list = sdr_list_head; list != NULL; list = next) {
		switch (list->type) {
		case SDR_RECORD_TYPE_FULL_SENSOR:
			if (list->record.full)
				free(list->record.full);
			break;
		case SDR_RECORD_TYPE_COMPACT_SENSOR:
			if (list->record.compact)
				free(list->record.compact);
			break;
		case SDR_RECORD_TYPE_EVENTONLY_SENSOR:
			if (list->record.eventonly)
				free(list->record.eventonly);
			break;
		case SDR_RECORD_TYPE_GENERIC_DEVICE_LOCATOR:
			if (list->record.genloc)
				free(list->record.genloc);
			break;
		case SDR_RECORD_TYPE_FRU_DEVICE_LOCATOR:
			if (list->record.fruloc)
				free(list->record.fruloc);
			break;
		case SDR_RECORD_TYPE_MC_DEVICE_LOCATOR:
			if (list->record.mcloc)
				free(list->record.mcloc);
			break;
		case SDR_RECORD_TYPE_ENTITY_ASSOC:
			if (list->record.entassoc)
				free(list->record.entassoc);
			break;
		}
		next = list->next;
		free(list);
	}

	sdr_list_head = NULL;
	sdr_list_tail = NULL;
	sdr_list_itr = NULL;
}

