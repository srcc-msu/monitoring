#ifndef _NM_CONTROL_H_
#define _NM_CONTROL_H_

// We need struct timeval
#include <sys/time.h>

#define NM_COOKIE_SIZE		4
#define NM_SIGNATURE_SIZE	8
#define NM_MAX_ADDR_SIZE	16

#define NM_MSG_TYPE_TLV		1
#define NM_STRM_NUM_MON		1
#define NM_STRM_NUM_STAT	2
#define NM_STRM_NUM_HOPSA	4

#define NM_TLV_END_SIZE		4
#define NM_HMAC_SIZE		32

union laddr_u {
	uint8_t		b1[NM_MAX_ADDR_SIZE];
	uint32_t	b4[NM_MAX_ADDR_SIZE/sizeof(uint32_t)];
};

// The structure nm_kcmd_t is used to configure kernel thread using ioctl
// KNM_IOCTL. One shall keep them (ioctl value, structure size, field
// offsets and sizes) in sync with definitions in kernel.
// Also, this is identical to part of configuration message of version 1
// after signature and version field.
struct nm_kcmd {
	uint16_t	flags;			/* flags */
	union laddr_u	client_host;		/* configured host */
	union laddr_u	tgt_host;		/* target host */
	uint16_t	tgt_port;		/* target port */
	uint8_t		cookie[NM_COOKIE_SIZE];		/* new cookie */
} __attribute__((packed));
typedef struct nm_kcmd nm_kcmd;

// Common header for all kinds of control messages.
struct nm_control_header {
	uint8_t			signature[NM_SIGNATURE_SIZE];	/* signature */
	uint16_t		version;	/* version */
} __attribute__((packed));
typedef struct nm_control_header nm_control_header;

// Control message, version 1. The head shall be byte compatible with
// nm_control_header, as same as for other kinds of control messages.
struct nm_control_v1 {
	nm_control_header	common_header;
	struct nm_kcmd		command;
} __attribute__((packed));
typedef struct nm_control_v1 nm_control_v1;

struct nm_control_v2_header {
	nm_control_header	common_header;
	uint16_t		flags;
	uint8_t			signature_type;
	uint8_t			resp_code;
} __attribute__((packed));
typedef struct nm_control_v2_header nm_control_v2_header;

#define KNM_IOCTL	_IOW('X', 0x01, struct nm_kcmd)

enum {
	CAC_MODE_EXACT	= 0,
	CAC_MODE_IFACE	= 1,
	CAC_MODE_ANY	= 2
};

struct nm_data_hdr_t {
	uint8_t		signature[NM_SIGNATURE_SIZE];
	uint16_t	version;
	uint8_t		msg_type;
	uint8_t		strm_num;
	uint16_t	flags;
	union laddr_u	client_host;
	uint32_t	ssrc;
	uint32_t	seq_num;
	uint8_t		cookie[NM_COOKIE_SIZE];
	uint32_t	ts_m;
	uint32_t	ts_sec;
	uint32_t	ts_usec;
}__attribute__((packed));

struct nm_strm_buf_t {
	uint8_t		num;
	uint16_t	period;
	uint32_t	flags_mask;
	uint32_t	seq_num;
	int32_t		window;
	struct timeval	next_send;
	size_t		data_len;
	uint8_t		*data;
};

#ifdef _USERLAND
// List of streams supported by the agent.
extern struct nm_strm_buf_t *send_strms;
#endif

#ifdef _USERLAND
#define IS_NOT_EMPTY_STRM(S)	S->flags_mask
#endif /* _USERLAND */

typedef struct nm_server_state {
	int				sock;
	struct nm_kcmd			*cmd_hdr;
#ifdef _USERLAND
	struct nm_cpu_info_t		*cpu_info;
	struct nm_mem_info_t		*mem_info;
	struct nm_iface_info_t		*iface_info;
	struct nm_hw_status_t		*hwstat;
#else
	int				nm_ctl;
#endif
	int				cac_mode;
	int				ctladdr_fixed;
} nm_server_state;

void read_and_process_control_packet(nm_server_state*);
void update_strm(struct nm_data_hdr_t*, nm_kcmd*);

#endif
