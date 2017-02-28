#define _GNU_SOURCE
#define _BSD_SOURCE

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <string.h>
#include <errno.h>
#include "nm_syslog.h"

#include "nm_control.h"

#define CTL_PACKET_BUFFER_SIZE 65535

static const char *nm_ctl_sig  = "MMCS.ACF";
static const char *nm_resp_sig = "MMCS.ACR";

#define V2RESP_REQUISITE_FAILED		(1u<<0)
#define V2RESP_BAD_OPTION_VALUE		(1u<<1)
#define V2RESP_UNKNOWN_OPTION		(1u<<2)

int
nm_is_iface_address(struct ifaddrs *if_addrs,
    unsigned family, const void *address)
{
	// The address shall be checked against currently known list of
	// interface addresses.
	// We can compare only IPv{4,6} addresses.
	struct ifaddrs *ifa;
	for (ifa = if_addrs; ifa != NULL; ifa = ifa->ifa_next)
	{
		if (ifa->ifa_addr != NULL &&
		    ifa->ifa_addr->sa_family == family)
		{
			if (family == AF_INET) {
				struct sockaddr_in* sia =
				    (struct sockaddr_in*) ifa->ifa_addr;
				if (!memcmp(address, &sia->sin_addr,
					    sizeof(sia->sin_addr)))
					return 1;
			}
			if (family == AF_INET6) {
				struct sockaddr_in6* sis =
				    (struct sockaddr_in6*) ifa->ifa_addr;
				if (!memcmp(address, &sis->sin6_addr,
					    sizeof(sis->sin6_addr)))
					return 1;
			}
		}
	}
	return 0;	// not found or unsupported family
}

static unsigned
v1_flags_to_address_family(unsigned flags)
{
	switch (flags & 0x03) {
		case 0:		return AF_INET;
		case 1:		return AF_INET6;
		// FIXME: add InfiniBand
		default:	return 0;
	}
}

static void
set_v1_flags_by_address_family(nm_kcmd *kcmd, unsigned family)
{
	// As this deals with network packed data, it shall take care
	// of endianness.
	unsigned oldflags_masked = ntohs(kcmd->flags) & ~3u;
	if (family == AF_INET)
		kcmd->flags = htons(oldflags_masked);
	else if (family == AF_INET6)
		kcmd->flags = htons(1 | oldflags_masked);
}

static int
check_control_address(
    nm_server_state *sst,
    unsigned family,
    const void *wire_address)
{
	int rc;

	if (family != AF_INET && family != AF_INET6)
		return 0;
	if (sst->cac_mode == CAC_MODE_ANY)
		return 1;
	if (sst->cac_mode == CAC_MODE_IFACE) {
		struct ifaddrs* if_addrs;
		if (0 != getifaddrs(&if_addrs)) {
			// XXX log this error
			return 0;
		}
		rc = nm_is_iface_address(if_addrs,
		    family, wire_address);
		freeifaddrs(if_addrs);
		return rc;
	}
	// Exact case, direct address comparing.
	unsigned kcmd_family = v1_flags_to_address_family(
	     sst->cmd_hdr->flags);
	if (family != kcmd_family)
		return 0;
	if (family == AF_INET) {
		return 0 == memcmp(
		    &sst->cmd_hdr->client_host, wire_address, 4);
	}
	if (family == AF_INET6) {
		return 0 == memcmp(
		    &sst->cmd_hdr->client_host, wire_address, 16);
	}
	return 0;
}

static void
control_apply(struct nm_server_state *sst,
		unsigned streamno,
		int window_defined,
		int window)
{
#ifdef _USERLAND
	struct nm_strm_buf_t *strm;
#endif

#ifndef _USERLAND
	ioctl(sst->nm_ctl, KNM_IOCTL, sst->cmd_hdr);
#else
	for (strm = send_strms; IS_NOT_EMPTY_STRM(strm); strm++){
		if (streamno != 0 && streamno != (int) strm->num)
			continue;
		if (strm->data)
			update_strm(
				(struct nm_data_hdr_t *)strm->data,
				 sst->cmd_hdr);
		if (window_defined)
			strm->window = window;
	}
#endif
}

static int
v2_process_dest_cookie(unsigned char *tp, size_t elen,
		struct nm_kcmd *new_cmd_hdr,
		nm_server_state *sst)
{
	if (elen != NM_COOKIE_SIZE) {
		nm_syslog(LOG_NOTICE,
			"process_control_v2: bad_cookie_length");
		return V2RESP_BAD_OPTION_VALUE;
	}
	memcpy(new_cmd_hdr->cookie, tp, NM_COOKIE_SIZE);
	return 0;
}

static int
v2_process_agrhost(
	struct nm_kcmd *new_cmd_hdr,
	unsigned char *tp,
	size_t elen)
{
	// FIXME: IPv4 only yet => 0, 0 as type and 4 octets as contents
	if (elen != 6 || tp[0] != 0 || tp[1] != 0) {
		nm_syslog(LOG_NOTICE, "process_control_v2:"
			" bad aggregator host length or type");
		return V2RESP_BAD_OPTION_VALUE;
	}
	memcpy((char*)&new_cmd_hdr->tgt_host, &tp[2], 4);
	memset((char*)&new_cmd_hdr->tgt_host + 4, 0, 12);
	return 0;
}

static int
v2_process_agent_host(
	struct nm_kcmd *new_cmd_hdr,
	unsigned char *tp,
	size_t elen,
	nm_server_state *sst)
{
	unsigned char hostbuf[16];
	// FIXME: IPv4 only yet => 0, 0 as type and 4 octets as contents
	if (elen != 6 || tp[0] != 0 || tp[1] != 0) {
		nm_syslog(LOG_NOTICE, "process_control_v2:"
			" bad agent host length or type");
		return V2RESP_BAD_OPTION_VALUE;
	}
	unsigned xfam = v1_flags_to_address_family(0);
	memcpy(hostbuf, &tp[2], 4);
	memset(hostbuf + 4, 0, 12);
	if (!check_control_address(sst, xfam, &hostbuf))
	{
		nm_syslog(LOG_WARNING, "misrouted control packet, ignoring");
		return V2RESP_REQUISITE_FAILED;
	}
	memcpy(&new_cmd_hdr->client_host, hostbuf, 16);
	return 0;
}

static int
v2_process_stream(
	unsigned *streamp,
	unsigned char *tp,
	size_t elen)
{
	unsigned stream;
	if (elen != 2) {
		nm_syslog(LOG_NOTICE, "process_control_v2: bad stream");
		return V2RESP_BAD_OPTION_VALUE;
	}
	stream = ntohs(*(uint16_t*) tp);
	if (stream != 0 && stream != NM_STRM_NUM_MON &&
		stream != NM_STRM_NUM_STAT && stream != NM_STRM_NUM_HOPSA)
	{
		nm_syslog(LOG_WARNING, "invalid stream, ignoring");
		return V2RESP_BAD_OPTION_VALUE;
	}
	*streamp = stream;
	return 0;
}

static int
v2_process_window(
	int *windowp,
	unsigned char *tp,
	size_t elen)
{
	int window;
	if (elen != 4) {
		nm_syslog(LOG_NOTICE, "process_control_v2: bad window");
		return V2RESP_BAD_OPTION_VALUE;
	}
	window = (int) ntohl(*(uint32_t*) tp);
	if (window != -1 && window < 0)
	{
		nm_syslog(LOG_WARNING, "invalid window, ignoring");
		return V2RESP_BAD_OPTION_VALUE;
	}
	*windowp = window;
	return 0;
}

static void
process_control_v2(char *pbuf, ssize_t pbsize,
    struct sockaddr_in *rcaddr_p,
    socklen_t rcaddr_len,
    nm_server_state *sst)
{
	int r, bad;
	nm_control_v2_header *v2h = (nm_control_v2_header*) pbuf;
	ssize_t nfrc;
	nm_kcmd new_cmd_hdr;
	unsigned stream = 0;
	int window = 0, window_defined = 0;

	if (v2h->flags != 0) {
		nm_syslog(LOG_NOTICE,
			"process_control_v2: control packet with flags!=0 -> ignore");
		return;
	}
	if (v2h->resp_code != 0) {
		nm_syslog(LOG_NOTICE,
			"process_control_v2:"
			" control packet with resp_code!=0 -> ignore");
		return;
	}
	if (pbsize < 4 + sizeof(*v2h)) {
		// It shall have at least 4 zero bytes to terminate TLV list
		nm_syslog(LOG_NOTICE,
			"process_control_v2:"
			" control packet too short -> ignore");
		return;
	}
	memcpy(&new_cmd_hdr, sst->cmd_hdr, sizeof(new_cmd_hdr));
	// Iterate TLV list.
	bad = 0;
	unsigned char *tp = (unsigned char*) pbuf + sizeof(*v2h);
	unsigned char *plimit = (unsigned char*) pbuf + pbsize;
	for(;;) {
		if (plimit - tp < 4) {
			nm_syslog(LOG_NOTICE,
				"process_control_v2: premature TLV finish (1)");
			return;
		}
		unsigned elen = (tp[0] << 8) + tp[1];
		unsigned etype = (tp[2] << 8) + tp[3];
		tp += 4;
		if (elen == 0)
			break;		// TLV finish
		if (plimit - tp < elen) {
			nm_syslog(LOG_NOTICE,
				"process_control_v2: premature TLV finish (2)");
			return;
		}
		int ignorable = (0 != (etype & 0x8000));
		etype &= ~0x8000;
		// XXX process the element
		// XXX Properly treat ignorables
		if (etype == 1) {
			bad |= v2_process_agrhost(
				&new_cmd_hdr, tp, elen);
		}
		else if (etype == 2) {
			if (elen == 2) {
				new_cmd_hdr.tgt_port = *(uint16_t*) tp;
			} else {
				nm_syslog(LOG_NOTICE, "process_control_v2: destination port length is invalid");
				bad |= V2RESP_BAD_OPTION_VALUE;
			}
		}
		else if (etype == 3) {
			bad |= v2_process_dest_cookie(
				tp, elen, &new_cmd_hdr, sst);
		}
		else if (etype == 5) {
			r = v2_process_window(&window, tp, elen);
			if (r == 0)
				window_defined = 1;
			else
				bad |= r;
		}
		else if (etype == 256) {
			bad |= v2_process_agent_host(
				&new_cmd_hdr, tp, elen, sst);
		}
		else if (etype == 257) {
			bad |= v2_process_stream(
				&stream, tp, elen);
		}
		else {
			if (!ignorable) {
				bad |= V2RESP_UNKNOWN_OPTION;
			}
		}
		tp += elen;
	}
	// TODO: Check the signature, if requested
	// Here, the whole parsing succeeded. Apply the changes.
	if (bad == 0) {
		memcpy(sst->cmd_hdr, &new_cmd_hdr, sizeof(new_cmd_hdr));
		control_apply(sst, stream, window_defined, window);
	}
	else {
		nm_syslog(LOG_WARNING, "error processing v2 control packet:"
			" %02x", bad);
	}
	// Respond with the same packet size.
	// TODO: when signatures will be supported, recalculate the one.
	v2h->resp_code = (bad != 0);
	memcpy(v2h->common_header.signature, nm_resp_sig, NM_SIGNATURE_SIZE);
	nfrc = sendto(sst->sock, pbuf, pbsize, 0,
	    (struct sockaddr *) rcaddr_p, rcaddr_len);
	if (nfrc == -1) {
		nm_syslog(LOG_WARNING,
		    "error on responding to control packet: %s",
		    strerror(errno));
	}
}

static void
process_control_v1(char *pbuf, ssize_t pbsize,
    struct sockaddr_in *rcaddr_p,
    socklen_t rcaddr_len,
    nm_server_state *sst)
{
	struct nm_kcmd *cmd_hdr = sst->cmd_hdr;
	ssize_t nfrc;

	if (pbsize < (ssize_t)sizeof(nm_control_v1)) {
		nm_syslog(LOG_WARNING,
		    "version 1 control packet but too short (%zd)", pbsize);
		return;
	}
	nm_control_v1 *v1 = (nm_control_v1 *)pbuf;
	// TODO: Flags processing (IPV6, HMAC security, etc.)
	// Check whether the control address is appropriate.
	// Is this our address?
	unsigned xfam = v1_flags_to_address_family(ntohs(v1->command.flags));
	if (!check_control_address(sst, xfam, &v1->command.client_host))
	{
		nm_syslog(LOG_WARNING, "misrouted control packet, ignoring");
		return;
	}
	if (!sst->ctladdr_fixed) {
		// Address shall be changed according to incoming command.
		set_v1_flags_by_address_family(cmd_hdr, xfam);
		cmd_hdr->client_host = v1->command.client_host;
	}
	cmd_hdr->tgt_host = v1->command.tgt_host;
	cmd_hdr->tgt_port = v1->command.tgt_port;
	memcpy(&cmd_hdr->cookie, &v1->command.cookie, sizeof(cmd_hdr->cookie));
	control_apply(sst, 0, 0, 0);
	memcpy(v1->common_header.signature, nm_resp_sig, NM_SIGNATURE_SIZE);
	// Respond with the same packet size.
	// TODO: when signatures will be supported, recalculate the one.
	nfrc = sendto(sst->sock, pbuf, pbsize, 0,
	    (struct sockaddr *) rcaddr_p, rcaddr_len);
	if (nfrc == -1) {
		nm_syslog(LOG_WARNING,
		    "error on responding to control packet: %s",
		    strerror(errno));
	}
}

void
read_and_process_control_packet(nm_server_state *sst)
{
	struct sockaddr_in rcaddr;
	char pbuf[CTL_PACKET_BUFFER_SIZE];
	socklen_t rcaddr_len = sizeof(struct sockaddr_in);
	ssize_t nfrc;	// network function return code
	nm_control_header* common_header = (nm_control_header*) pbuf;
	unsigned version;

	// We receive packet of maximal possible size and then check version
	// and apply parsing accordingly.
	// Version 1 of control packet has fixed size (50 octets before
	// checksum). Version 2 has TLV list of variable size.
	nfrc = recvfrom(sst->sock, pbuf, CTL_PACKET_BUFFER_SIZE, 0,
	    (struct sockaddr *)&rcaddr, &rcaddr_len);
	if (nfrc == -1) {
		if (errno != EINTR && errno != EWOULDBLOCK)
			nm_syslog(LOG_WARNING,
			    "error on reading control packet: %s",
			    strerror(errno));
		return;
	}
	nm_syslog(LOG_INFO, "got packet on control socket, length=%zd", nfrc);
	if (nfrc < sizeof(nm_control_header))
		return;	// something too short
	if (0 != memcmp(common_header->signature, nm_ctl_sig,
	    NM_SIGNATURE_SIZE))
	{
		nm_syslog(LOG_WARNING, "invalid control packet signature");
		return;
	}
	version = ntohs(common_header->version);
	if (version == 1) {
		process_control_v1(pbuf, nfrc, &rcaddr, rcaddr_len, sst);
		return;
	}
	if (version == 2) {
		process_control_v2(pbuf, nfrc, &rcaddr, rcaddr_len, sst);
		return;
	}
	// Unknown version => ignore
	nm_syslog(LOG_WARNING, "unsupported version %u of the packet configuration", version);
}
