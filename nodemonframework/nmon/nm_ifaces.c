#define _BSD_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nm_dumb.h"
#include "nm_module.h"


/**************** Network interfaces routines ****************/

#define NETIF_FILE	"/proc/net/dev"


#define IF_ELM_LEN 			sizeof(uint64_t)


#define IDX_RX_P	0
#define IDX_TX_P	1
#define IDX_RX_B	2
#define IDX_TX_B	3
#define IDX_RX_E	4
#define IDX_TX_E	5
#define IDX_RX_D	6
#define IDX_TX_D	7
#define IDX_MULT	8
#define IDX_COLL	9
#define IDX_NUM		10


static void init_ifs_hdrs(struct nm_iface_info_t *info){
	int i;

	for (i=0; i < IDX_NUM; i++){
		*(struct nm_tlv_hdr_t *)NM_GROUPADDR(info->dynval, IF_ELM_LEN, info->num, i) = 
		    NM_MONTYPE(MON_RX_PACKETS + i, IF_ELM_LEN * info->num);
	}
}


int nm_init_ifaces(char *ifsstr, struct nm_iface_info_t *info){
	char *tmp, *buf;
	int se, i, ifcnt;
	FILE *f;

	i = ifcnt = 0;

	if (!(ifsstr && info))
		return 0;

	//Count number of interfaces
	tmp = ifsstr;
	while ((tmp = strchr(tmp, ','))){
		ifcnt++;
		tmp++;
	}
	ifcnt++;

	info->dynval = malloc(NM_DYNELEMLEN(IF_ELM_LEN, ifcnt) * IDX_NUM);
	if (!(info->dynval)) {
cleanup:
		// Save errno for the last error; caller routine would use it
		// to detect error kind and set exit code.
		se = errno;
		nm_release_ifaces(info);
		errno = se;
		return -1;
	}

	if (!(buf = malloc(BUFSIZE)))
		goto cleanup;

	bzero(info->dynval, NM_DYNELEMLEN(IF_ELM_LEN, ifcnt) * IDX_NUM);

	if (!(f = fopen(NETIF_FILE, "r")))
		goto cleanup;

	info->names = malloc(sizeof(char *) * ifcnt);
	if (!info->names)
		goto cleanup;

	while((tmp = strtok((i ? NULL : ifsstr), ","))){
		rewind(f);
		while (!feof(f)){
			fgets(buf, BUFSIZE, f);
			if (strstr(buf, tmp)){
				info->names[i++] = strdup(tmp);
				goto nxt_if;
			}
		}
		// Interface not found. This is fatal error
		ifcnt = -1;
		break;
nxt_if:		;
	}

	fclose(f);
	free(buf);

	if (ifcnt == -1){
		nm_release_ifaces(info);
		errno = ENOENT;
	} else {
		info->num = ifcnt;
		info->dynlen = NM_DYNELEMLEN(IF_ELM_LEN, ifcnt) * IDX_NUM;
		init_ifs_hdrs(info);
	}

	return ifcnt;
}


void nm_release_ifaces(struct nm_iface_info_t *info){
	int i;

	if (!info)
		return;

	if (info->dynval){
		free(info->dynval);
		info->dynval = NULL;
		info->dynlen = 0;
	}

	if (info->names){
		for (i=0; i < info->num; i++){
			free(info->names[i]);
		}
		info->names = NULL;
	}

	info->num = 0;
}


int nm_getinfo_ifaces(struct nm_iface_info_t *info){
	int i ,j;
	uint64_t unused;
	FILE *f;
	char *buf, *tmp;

	if (!info)
		return 0;

	if (!info->dynval)
		return 0;

#define IVADDR(I)	NM_VECTADDR(info->dynval, IF_ELM_LEN, info->num, I, uint64_t)
	for (i = 0; i < info->num; i++)
		for (j = 0; j<IDX_NUM; j++)
			IVADDR(j)[i] = htonll(NM_CNT_ERR);

	if (!(f = fopen(NETIF_FILE, "r")))
		return -1;
	
	if (!(buf = malloc(BUFSIZE))){
		goto err;
	}

	while (!feof(f)){
		fgets(buf, BUFSIZE, f);
		for (i = 0; i < info->num; i++){
			tmp = strstr(buf, info->names[i]);
			if (!tmp)
				continue;

			tmp = strchr(tmp, ':');
			tmp++;

			sscanf(tmp,
#if __WORDSIZE == 64
			    "%lu%lu%lu%lu%lu%lu%lu%lu%lu%lu%lu%lu%lu%lu%lu%lu",
#else
			    "%llu%llu%llu%llu%llu%llu%llu%llu%llu%llu%llu%llu%llu%llu%llu%llu",
#endif
			    &(IVADDR(IDX_RX_B)[i]),
			    &(IVADDR(IDX_RX_P)[i]),
			    &(IVADDR(IDX_RX_E)[i]),
			    &(IVADDR(IDX_RX_D)[i]),
			    &unused, &unused, &unused, 
			    &(IVADDR(IDX_MULT)[i]),
			    &(IVADDR(IDX_TX_B)[i]),
			    &(IVADDR(IDX_TX_P)[i]),
			    &(IVADDR(IDX_TX_E)[i]),
			    &(IVADDR(IDX_TX_D)[i]),
			    &unused,
			    &(IVADDR(IDX_COLL)[i]),
			    &unused, &unused
			    );

			for (j = 0; j<IDX_NUM; j++)
				IVADDR(j)[i] = htonll(NM_GET_CNT64(IVADDR(j)[i]));

			break;
		}
	}
#undef IVADDR
	
	free(buf);
err:
	fclose(f);
	
	return 0;
}



