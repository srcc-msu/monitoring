#ifndef _NMPE_H_
#define _NMPE_H_

#include <stdint.h>

enum {
	TPCNT_SW_CPU_CLOCK_IDX = 0,
	TPCNT_SW_TASK_CLOCK_IDX,
	TPCNT_SW_PFLT_IDX,
	TPCNT_SW_CTX_SW_IDX,
	TPCNT_SW_CPU_MIG_IDX,
	TPCNT_SW_PFLT_MIN_IDX,
	TPCNT_SW_PFLT_MAJ_IDX,

	TPCNT_SW_NUM,
};


enum {
	CPU_PERF_FIXED01_IDX = 0,
	CPU_PERF_FIXED02_IDX,
	CPU_PERF_FIXED03_IDX,
	CPU_PERF_COUNTER01_IDX,
	CPU_PERF_COUNTER02_IDX,
	CPU_PERF_COUNTER03_IDX,
	CPU_PERF_COUNTER04_IDX,
	CPU_PERF_COUNTER05_IDX,
	CPU_PERF_COUNTER06_IDX,
	CPU_PERF_COUNTER07_IDX,
	CPU_PERF_COUNTER08_IDX,

	CPU_PERF_NUM,
};


/* SW configuration flags */
#define NMPE_SW_CONF_USERSPACE		(1ULL << 0)
#define NMPE_SW_CONF_KERNELSPACE	(1ULL << 1)
#define NMPE_SW_CONF_HYPERVISOR		(1ULL << 2)
#define NMPE_SW_CONF_IDLE		(1ULL << 3)


int nmpe_init();
int nmpe_open_sw(int id, uint64_t conf);
int nmpe_open_raw(int id, uint64_t conf);
int nmpe_read_sw(int id, uint64_t *buf);
int nmpe_read_raw(int id, uint64_t *buf);
int nmpe_close_sw(int id);
int nmpe_close_raw(int id);
void nmpe_free();


#endif /* _NMPE_H_ */

