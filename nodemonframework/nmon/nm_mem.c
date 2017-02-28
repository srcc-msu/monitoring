#define _BSD_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "nm_dumb.h"
#include "nm_module.h"
#include "nm_syslog.h"


#define _STRINGIFY(s)	#s
#define STRINGIFY(s)	_STRINGIFY(s)


#define MEMINFO_FILE	"/proc/meminfo"
#define VMSTAT_FILE	"/proc/vmstat"

#define MEM_ELM_LEN	sizeof(uint64_t)
#define HTLB_ELM_LEN	sizeof(uint32_t)
#define VM_ELM_LEN	sizeof(uint64_t)

#define MEM_ERR		-1
#define HTLB_ERR	-1
#define VM_ERR		NM_CNT_ERR

enum {
	MEMORY_TOTAL_IDX = 0,
	MEMORY_USED_IDX,
	MEMORY_SWAP_TOTAL_IDX,
	MEMORY_SWAP_FREE_IDX,
	MEMORY_TOTAL_FREE_IDX,
	MEMORY_FREE_IDX,
	MEMORY_BUFFERS_IDX,
	MEMORY_CACHED_IDX,
	MEMORY_MLOCKED_IDX,

	MEMORY_IDX_MAX
};

enum {
	HUGETLB_TOTAL_IDX = 0,
	HUGETLB_FREE_IDX,
	HUGETLB_RESV_IDX,

	HUGETLB_IDX_MAX
};

enum {
	VM_PGPGIN_IDX = 0,
	VM_PGPGOUT_IDX,
	VM_PSWPIN_IDX,
	VM_PSWPOUT_IDX,

	VM_IDX_MAX
};

static void init_mem_hdr(int num_elem, int first_sid, size_t elem_len, uint8_t *dynval){
	int i;

	for (i = 0; i < num_elem; i++){
		*(struct nm_tlv_hdr_t *)NM_GROUPADDR(dynval, elem_len, 1, i) =
		    NM_MONTYPE(first_sid + i, elem_len);
	}
}


static void init_mem_hdrs(struct nm_mem_info_t *info){

	init_mem_hdr(MEMORY_IDX_MAX, MON_MEMORY_TOTAL, MEM_ELM_LEN, info->dynvalmem);
	init_mem_hdr(HUGETLB_IDX_MAX, MON_HUGETLB_TOTAL, HTLB_ELM_LEN, info->dynvalhtlb);
	init_mem_hdr(VM_IDX_MAX, MON_VM_PGPGIN, VM_ELM_LEN, info->dynvalvm);
}


int nm_init_mem(struct nm_mem_info_t *info){
	size_t total_len;

	if (!info)
		return -1;
	
	info->dynmlen = NM_DYNELEMLEN(MEM_ELM_LEN, 1) * MEMORY_IDX_MAX;
	info->dynhlen = NM_DYNELEMLEN(HTLB_ELM_LEN, 1) * HUGETLB_IDX_MAX;
	info->dynvlen = NM_DYNELEMLEN(VM_ELM_LEN, 1) * VM_IDX_MAX;

	total_len = info->dynmlen + info->dynhlen + info->dynvlen;

	if (!(info->dynvalmem = malloc(total_len))){
		nm_syslog(LOG_ERR, "cannot allocate memory: %s",
							strerror(errno));
		return -1;
	}

	info->dynvalhtlb = info->dynvalmem + info->dynmlen;
	info->dynvalvm = info->dynvalhtlb + info->dynhlen;
	
	bzero(info->dynvalmem, total_len);

	init_mem_hdrs(info);
	
	return 0;
}


void nm_release_mem(struct nm_mem_info_t *info){
	if (!info)
		return;
	
	if (info->dynvalmem){
		free(info->dynvalmem);
		info->dynvalmem = NULL;
	}
	info->dynmlen = 0;
	
	info->dynvalhtlb = NULL;
	info->dynhlen = 0;

	info->dynvalvm = NULL;
	info->dynvlen = 0;
}


struct stat {
	const char	*str;
	uint64_t	value;
};

typedef uint32_t mask_t;
#define MASK(idx) (((mask_t)1) << (idx))


#define STAT_STR_LEN 30
static void nm_parse_file(const char *fname,
			struct stat *stats,
			int stats_len,
			mask_t *mask){
	int i;
	char line[4 * STAT_STR_LEN];
	char str[STAT_STR_LEN + 1];
	long long unsigned val;
	mask_t all_stats = MASK(stats_len) - 1;
	FILE *f;

	if (!(f = fopen(fname, "r"))){
		nm_syslog(LOG_ERR, "cannot open file %s: %s", fname,
							strerror(errno));
		return;
	}

	while (fgets(line, sizeof(line), f)){
		sscanf(line, "%" STRINGIFY(STAT_STR_LEN) "s %llu", str, &val);
		for (i = 0; i < stats_len; i++){
			if (*mask & MASK(i))
				continue;

			if (strcmp(str, stats[i].str))
				continue;

			stats[i].value = val;
			*mask |= MASK(i);
			break;
		}

		if (*mask == all_stats)
			break;
	}

	fclose(f);
}
#undef STAT_STR_LEN


static void nm_get_meminfo(struct nm_mem_info_t *info){
	uint64_t tmp;
	mask_t mask = 0;
	enum {
		MEMINFO_TOTAL_IDX = 0,
		MEMINFO_SWAP_TOTAL_IDX,
		MEMINFO_SWAP_FREE_IDX,
		MEMINFO_CACHED_IDX,
		MEMINFO_BUFFERS_IDX,
		MEMINFO_FREE_IDX,
		MEMINFO_MLOCKED_IDX,

		MEMINFO_HPTOTAL_IDX,
		MEMINFO_HPFREE_IDX,
		MEMINFO_HPRESV_IDX,

		MEMINFO_IDX_MAX
	};
#define S(STAT_IDX, STR)		\
		[STAT_IDX] = {		\
			.str = STR,	\
		}
	struct stat stats[MEMINFO_IDX_MAX] = {
		S(MEMINFO_TOTAL_IDX,		"MemTotal:"),
		S(MEMINFO_SWAP_TOTAL_IDX,	"SwapTotal:"),
		S(MEMINFO_SWAP_FREE_IDX,	"SwapFree:"),
		S(MEMINFO_CACHED_IDX,		"Cached:"),
		S(MEMINFO_BUFFERS_IDX,		"Buffers:"),
		S(MEMINFO_FREE_IDX,		"MemFree:"),
		S(MEMINFO_MLOCKED_IDX,		"Mlocked:"),
		S(MEMINFO_HPTOTAL_IDX,		"HugePages_Total:"),
		S(MEMINFO_HPFREE_IDX,		"HugePages_Free:"),
		S(MEMINFO_HPRESV_IDX,		"HugePages_Rsvd:")
	};
#undef S

	nm_parse_file(MEMINFO_FILE, stats, MEMINFO_IDX_MAX, &mask);

#define SET_MSENSOR(IDX, VAL)						 \
	NM_VECTADDR(info->dynvalmem, MEM_ELM_LEN, 1, IDX, uint64_t)[0] = \
	    htonll(VAL);

#define SET_MSENSOR_ERR(IDX) SET_MSENSOR(IDX, MEM_ERR)
#define S(IDX)								   \
	if (mask & MASK(MEMINFO_ ## IDX)){				   \
		SET_MSENSOR(MEMORY_ ## IDX, stats[MEMINFO_ ## IDX].value); \
	} else {							   \
		SET_MSENSOR_ERR(MEMORY_ ## IDX);			   \
	}
	S(TOTAL_IDX);
	S(SWAP_TOTAL_IDX);
	S(SWAP_FREE_IDX);
	S(CACHED_IDX);
	S(BUFFERS_IDX);
	S(FREE_IDX);
	S(MLOCKED_IDX);

	if (!(mask & (MASK(MEMINFO_FREE_IDX) | MASK(MEMINFO_BUFFERS_IDX)
		      | MASK(MEMINFO_CACHED_IDX)))){

		SET_MSENSOR_ERR(MEMORY_TOTAL_FREE_IDX);
	} else {
		tmp =	stats[MEMINFO_FREE_IDX].value +
			stats[MEMINFO_BUFFERS_IDX].value +
			stats[MEMINFO_CACHED_IDX].value;
		SET_MSENSOR(MEMORY_TOTAL_FREE_IDX, tmp);
	}

	if (!(mask & (MASK(MEMINFO_FREE_IDX) | MASK(MEMINFO_TOTAL_IDX)))){
		SET_MSENSOR_ERR(MEMORY_USED_IDX);
	} else {
		tmp =	stats[MEMINFO_TOTAL_IDX].value -
			stats[MEMINFO_FREE_IDX].value;
		SET_MSENSOR(MEMORY_USED_IDX, tmp);
	}
#undef S
#undef SET_MSENSOR_ERR
#undef SET_MSENSOR


#define SET_HSENSOR(IDX, VAL)						   \
	NM_VECTADDR(info->dynvalhtlb, HTLB_ELM_LEN, 1, IDX, uint32_t)[0] = \
	    htonl(VAL);

#define S(IDX)								      \
	if (mask & MASK(MEMINFO_HP ## IDX)){				      \
		SET_HSENSOR(HUGETLB_ ## IDX, stats[MEMINFO_HP ## IDX].value); \
	} else {							      \
		SET_HSENSOR(HUGETLB_ ## IDX, HTLB_ERR);			      \
	}
	S(TOTAL_IDX);
	S(FREE_IDX);
	S(RESV_IDX);
#undef S
#undef SET_HSENSOR
}


static void nm_get_vmstat(struct nm_mem_info_t *info){
	mask_t mask = 0;
#define S(STAT_IDX, STR)		\
		[STAT_IDX] = {		\
			.str = STR,	\
		}
	struct stat stats[VM_IDX_MAX] = {
		S(VM_PGPGIN_IDX,	"pgpgin"),
		S(VM_PGPGOUT_IDX,	"pgpgout"),
		S(VM_PSWPIN_IDX,	"pswpin"),
		S(VM_PSWPOUT_IDX,	"pswpout"),
	};
#undef S

	nm_parse_file(VMSTAT_FILE, stats, VM_IDX_MAX, &mask);

#define SET_VSENSOR(IDX, VAL)						\
	NM_VECTADDR(info->dynvalvm, VM_ELM_LEN, 1, IDX, uint64_t)[0] =	\
	    htonll(VAL);

#define S(IDX)								\
	if (mask & MASK(IDX)){						\
		SET_VSENSOR(IDX, NM_GET_CNT64(stats[IDX].value));	\
	} else {							\
		SET_VSENSOR(IDX, VM_ERR);				\
	}
	S(VM_PGPGIN_IDX);
	S(VM_PGPGOUT_IDX);
	S(VM_PSWPIN_IDX);
	S(VM_PSWPOUT_IDX);
#undef S
#undef SET_VSENSOR
}


void nm_getinfo_mem(struct nm_mem_info_t *info){
	nm_get_meminfo(info);
	nm_get_vmstat(info);
}

