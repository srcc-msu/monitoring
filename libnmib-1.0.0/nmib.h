#ifndef _NMIB_H_
#define _NMIB_H_


typedef struct {
	uint32_t symbolerrors;
	uint32_t linkrecovers;
	uint32_t linkdowned;
	uint32_t rcverrors;
	uint32_t rcvremotephyerrors;
	uint32_t rcvswrelayerrors;
	uint32_t xmtdiscards;
	uint32_t xmtconstrainterrors;
	uint32_t rcvconstrainterrors;
	uint32_t linkintegrityerrors;
	uint32_t excbufoverrunerrors;
	uint32_t vl15dropped;
	uint64_t xmtdata;
	uint64_t rcvdata;
	uint64_t xmtpkts;
	uint64_t rcvpkts;
	uint32_t xmtwait;
} nmib_counters_t;


/* nmib flags */
#define NMIB_IFACE_EXT_CNT    (1 << 0)
#define NMIB_IFACE_XMTWAIT    (1 << 1)

typedef struct {
	uint16_t	flags;
	nmib_counters_t	counters;
	void		*state;
} nmib_t;


typedef struct {
	int		port;
	char		*dev_name;
} nmib_attr_t;


nmib_t *nmib_open(nmib_attr_t *attr);
void nmib_close(nmib_t *nmib);
int nmib_read_counters(nmib_t *nmib);


#endif /* _NMIB_H_ */

