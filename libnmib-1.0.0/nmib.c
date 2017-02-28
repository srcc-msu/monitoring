#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <infiniband/mad.h>

#include "nmib.h"


#define NEED_RESET_MASK		((((uint32_t)1) << 31)|(((uint32_t)1) << 30))


/* Bits to clear the counters.
   See description in "InfiniBandTM Architecture Specification" 16.1.3.5 */
#define IB_PC_XMT_BYTES_RESET	(((unsigned)1) << 12)
#define IB_PC_RCV_BYTES_RESET	(((unsigned)1) << 13)
#define IB_PC_XMT_PKTS_RESET	(((unsigned)1) << 14)
#define IB_PC_RCV_PKTS_RESET	(((unsigned)1) << 15)


/* These macros are defined in the infiniband/iba/ib_types.h, but copied here
   to remove the build requires.
   See description in "InfiniBandTM Architecture Specification" 16.1.3.1 */
#define IB_PM_EXT_WIDTH_SUPPORTED	(htons(((uint16_t)1) << 9))
#define IB_PM_EXT_WIDTH_NOIETF_SUP	(htons(((uint16_t)1) << 10))
#define IB_PM_PC_XMIT_WAIT_SUP		(htons(((uint16_t)1) << 12))


/* Structure to simulate the extended counters.
   Used if the extension is not supported by hardware. */
typedef struct {
	uint32_t curr; /* current value */
	uint64_t accum; /* accumulate value */
} ext_counter_t;

typedef struct {
	ext_counter_t xmtdata;
	ext_counter_t rcvdata;
	ext_counter_t xmtpkts;
	ext_counter_t rcvpkts;
} ext_counters_t;


typedef struct {
	int			portnum;
	ib_portid_t		portid;
#ifndef OLD_IBMAD_COMPAT
	struct ibmad_port	*port;
#endif
} nmib_ibport_t;


typedef struct {
	nmib_t		nmib;
	nmib_ibport_t	ibport;
	ext_counters_t	ext_counters;
} nmib_state_t;

static uint8_t pc[1024]; /* buffer for pma query results */


static void nmib_close_ibport(nmib_ibport_t *ibport){
/* I have no idea how to close port of the old ibmad interface. */
#ifndef OLD_IBMAD_COMPAT
	mad_rpc_close_port(ibport->port);
#endif
}


void nmib_close(nmib_t *nmib){
	nmib_state_t *state = (nmib_state_t *)nmib->state;

	nmib_close_ibport(&state->ibport);
	free(state);
}


static int nmib_open_ibport(int port, char *dev_name, nmib_ibport_t *ibport){
	int mgmt_classes[4] = { IB_SMI_CLASS, IB_SMI_DIRECT_CLASS, IB_SA_CLASS,
				IB_PERFORMANCE_CLASS };

#ifndef OLD_IBMAD_COMPAT
	ibport->port = mad_rpc_open_port(dev_name, port, mgmt_classes, 4);

	if (!ibport->port){
		fprintf(stderr, "cannot to open '%s' port '%d'\n",
							dev_name, port);
		return -1;
	}
#else
	madrpc_init(dev_name, port, mgmt_classes, 4);
#endif

#ifndef OLD_IBMAD_COMPAT
	if (ib_resolve_self_via(&ibport->portid, &ibport->portnum, 0,
							ibport->port) < 0)
#else
	if (ib_resolve_self(&ibport->portid, &ibport->portnum, 0) < 0)
#endif
	{
		fprintf(stderr, "can't resolve self port '%s'\n", dev_name);
		nmib_close_ibport(ibport);
		return -1;
	}

	return 0;
}


static int nmib_get_ibportinfo(nmib_ibport_t *ibport, nmib_t *nmib){
	uint16_t cap_mask; /* capability mask */

	memset(pc, 0, sizeof(pc));
#ifndef OLD_IBMAD_COMPAT
	if (!pma_query_via(pc, &ibport->portid, ibport->portnum, 0,
						CLASS_PORT_INFO, ibport->port))
#else
	if (!perf_classportinfo_query(pc, &ibport->portid, ibport->portnum, 0))
#endif
	{
		fprintf(stderr, "query class port info failed\n");
		return -1;
	}
	memcpy(&cap_mask, pc + 2, sizeof(cap_mask));

	if ((cap_mask & IB_PM_EXT_WIDTH_SUPPORTED) ||
	    (cap_mask & IB_PM_EXT_WIDTH_NOIETF_SUP)){
		nmib->flags |= NMIB_IFACE_EXT_CNT;
	} else {
		fprintf(stdout, "no extended counters support indicated\n");
	}
#ifdef IB_PC_XMT_WAIT_F
#ifndef IGNORE_TEST_XMTWAIT
	if (cap_mask & IB_PM_PC_XMIT_WAIT_SUP){
#endif /* IGNORE_TEST_XMTWAIT */
		nmib->flags |= NMIB_IFACE_XMTWAIT;
#ifndef IGNORE_TEST_XMTWAIT
	} else {
		fprintf(stdout, "no XmitWait counter support indicated\n");
	}
#endif /* IGNORE_TEST_XMTWAIT */
#endif /* IB_PC_XMT_WAIT_F */
	return 0;
}


nmib_t *nmib_open(nmib_attr_t *attr){
	nmib_state_t *state;

	if (!(state = malloc(sizeof(nmib_state_t)))){
		perror("malloc");
		goto err_xit;
	}
	memset(state, 0, sizeof(nmib_state_t));

	if (nmib_open_ibport(attr->port, attr->dev_name, &state->ibport)){
		goto free_state;
	}

	if (nmib_get_ibportinfo(&state->ibport, &state->nmib)){
		goto close_ibport;
	}

	state->nmib.state = (void *)state;

	return &state->nmib;
close_ibport:
	nmib_close_ibport(&state->ibport);
free_state:
	free(state);
err_xit:
	return NULL;
}


int nmib_read_counters(nmib_t *nmib){
	unsigned reset_mask;
	nmib_counters_t *cntrs;
	ext_counters_t *ext_cntrs;
	nmib_ibport_t *ibport;
	nmib_state_t *state = (nmib_state_t *)nmib->state;

	ibport = &state->ibport;
	memset(pc, 0, sizeof(pc));
#ifndef OLD_IBMAD_COMPAT
	if (!pma_query_via(pc, &ibport->portid, ibport->portnum, 0,
					IB_GSI_PORT_COUNTERS, ibport->port))
#else
	if (!port_performance_query(pc, &ibport->portid, ibport->portnum, 0))
#endif
	{
		fprintf(stderr, "query port counters failed\n");
		return -1;
	}
	cntrs = &nmib->counters;
#define DF(FID, VAR) mad_decode_field(pc, FID, &(cntrs->VAR))
	DF(IB_PC_ERR_SYM_F,		symbolerrors);
	DF(IB_PC_LINK_RECOVERS_F,	linkrecovers);
	DF(IB_PC_LINK_DOWNED_F,		linkdowned);
	DF(IB_PC_ERR_RCV_F,		rcverrors);
	DF(IB_PC_ERR_PHYSRCV_F,		rcvremotephyerrors);
	DF(IB_PC_ERR_SWITCH_REL_F,	rcvswrelayerrors);
	DF(IB_PC_XMT_DISCARDS_F,	xmtdiscards);
	DF(IB_PC_ERR_XMTCONSTR_F,	xmtconstrainterrors);
	DF(IB_PC_ERR_RCVCONSTR_F,	rcvconstrainterrors);
	DF(IB_PC_ERR_LOCALINTEG_F,	linkintegrityerrors);
	DF(IB_PC_ERR_EXCESS_OVR_F,	excbufoverrunerrors);
	DF(IB_PC_VL15_DROPPED_F,	vl15dropped);
#ifdef IB_PC_XMT_WAIT_F
	if (nmib->flags & NMIB_IFACE_XMTWAIT){
		DF(IB_PC_XMT_WAIT_F,	xmtwait);
	}
#endif /* IB_PC_XMT_WAIT_F */

	if (nmib->flags & NMIB_IFACE_EXT_CNT){
		memset(pc, 0, sizeof(pc));
#ifndef OLD_IBMAD_COMPAT
		if (!pma_query_via(pc, &ibport->portid, ibport->portnum, 0,
				IB_GSI_PORT_COUNTERS_EXT, ibport->port))
#else
		if (!port_performance_ext_query(pc, &ibport->portid,
							ibport->portnum, 0))
#endif
		{
			fprintf(stderr, "query port counters ext failed\n");
			return -1;
		}
		DF(IB_PC_EXT_XMT_BYTES_F,	xmtdata);
		DF(IB_PC_EXT_RCV_BYTES_F,	rcvdata);
		DF(IB_PC_EXT_XMT_PKTS_F,	xmtpkts);
		DF(IB_PC_EXT_RCV_PKTS_F,	rcvpkts);
#undef DF
	} else {
		reset_mask = 0;
		ext_cntrs = &state->ext_counters;
#define S(SENS, VAR)							      \
		mad_decode_field(pc, SENS ## _F, &(ext_cntrs->VAR.curr));     \
		cntrs-> VAR = ext_cntrs-> VAR .accum + ext_cntrs-> VAR .curr; \
		if (ext_cntrs-> VAR .curr & NEED_RESET_MASK){		      \
			reset_mask |= SENS ## _RESET;			      \
			ext_cntrs-> VAR .accum += ext_cntrs-> VAR .curr;      \
		}

		S(IB_PC_XMT_BYTES, xmtdata)
		S(IB_PC_RCV_BYTES, rcvdata)
		S(IB_PC_XMT_PKTS, xmtpkts)
		S(IB_PC_RCV_PKTS, rcvpkts)
#undef S
		if (!reset_mask){
			return 0;
		}

		memset(pc, 0, sizeof(pc));
#ifndef OLD_IBMAD_COMPAT
		if (!performance_reset_via(pc, &ibport->portid,
					ibport->portnum, reset_mask, 0,
					IB_GSI_PORT_COUNTERS, ibport->port))
#else
		if (!port_performance_reset(pc, &ibport->portid,
					ibport->portnum, reset_mask, 0))
#endif
		{
			fprintf(stderr, "query reset counters failed\n");
			return -1;
		}
	}

	return 0;
}

