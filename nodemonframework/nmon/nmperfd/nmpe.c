#include <sys/syscall.h>
#include <linux/perf_event.h>
#include <unistd.h>
#include <errno.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <nm_syslog.h>
#include "nmpe.h"


#define sys_perf_event_open(attr, pid, cpu, group, flags) \
	syscall(__NR_perf_event_open, attr, pid, cpu, group, flags)


typedef struct {
	unsigned	active;
	int		*fds;
} nmpe_event_t;


static nmpe_event_t sw_event[TPCNT_SW_NUM] = {
	{0, NULL},
	{0, NULL},
	{0, NULL},
	{0, NULL},
	{0, NULL},
	{0, NULL},
	{0, NULL},
};

static nmpe_event_t raw_event[CPU_PERF_NUM] = {
	{0, NULL},
	{0, NULL},
	{0, NULL},
	{0, NULL},
	{0, NULL},
	{0, NULL},
	{0, NULL},
	{0, NULL},
	{0, NULL},
	{0, NULL},
	{0, NULL},
};

static int num_cpus = -1;


int nmpe_init(){
	const int num_events = TPCNT_SW_NUM + CPU_PERF_NUM;
	int i;
	int *fds_ptr;

	num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
	if (num_cpus < 0){
		nm_syslog(LOG_ERR, "nmpe_init: cannot get number of cpus: %s",
							strerror(errno));
		goto err_xit;
	}

	if (!(fds_ptr = malloc(sizeof(int) * num_cpus * num_events))){
		nm_syslog(LOG_ERR, "nmpe_init: cannot allocate memory: %s",
							strerror(errno));
		goto err_xit;
	}

	for (i = 0; i < TPCNT_SW_NUM; i++){
		sw_event[i].fds = fds_ptr;
		fds_ptr += num_cpus;
	}

	for (i = 0; i < CPU_PERF_NUM; i++){
		raw_event[i].fds = fds_ptr;
		fds_ptr += num_cpus;
	}

	return num_cpus;
err_xit:
	num_cpus = -1;
	return -1;
}


void nmpe_free(){
	int i;

	num_cpus = -1;

	if (sw_event[0].fds)
		free(sw_event[0].fds);

	for (i = 0; i < TPCNT_SW_NUM; i++){
		sw_event[i].fds = NULL;
	}

	for (i = 0; i < CPU_PERF_NUM; i++){
		raw_event[i].fds = NULL;
	}
}


static void close_perf_events(nmpe_event_t *event){
	int i;
	int *fds = event->fds;

	for (i = 0; i < num_cpus; i++){
		if (fds[i] != -1){
			close(fds[i]);
			fds[i] = -1;
		}
	}
	event->active = 0;
}


int nmpe_close_sw(int id){
	if ((id < 0) || (id >= TPCNT_SW_NUM)){
		nm_syslog(LOG_ERR, "nmpe_close_sw: invalid id '%d'", id);
		return -1;
	}
	close_perf_events(&sw_event[id]);
	return 0;
}


int nmpe_close_raw(int id){
	if ((id < 0) || (id >= CPU_PERF_NUM)){
		nm_syslog(LOG_ERR, "nmpe_close_raw: invalid id '%d'", id);
		return -1;
	}
	close_perf_events(&raw_event[id]);
	return 0;
}


static int open_perf_events(struct perf_event_attr *attr, nmpe_event_t *event){
	int i;
	int *fds = event->fds;

	for (i = 0; i < num_cpus; i++){
		fds[i] = -1;
	}

	for (i = 0; i < num_cpus; i++){
		fds[i] = sys_perf_event_open(attr, -1, i, -1, 0);
		if (fds[i] == -1){
			nm_syslog(LOG_ERR, "cannot open perf event: %s",
							strerror(errno));
			return -1;
		}
	}
	event->active = 1;

	return 0;
}


int nmpe_open_sw(int id, uint64_t conf){
#define S(NMPE_ID, PERF_ID) [NMPE_ID ## _IDX] = PERF_ID
	const enum perf_sw_ids perf_ids[TPCNT_SW_NUM] = {
		S(TPCNT_SW_CPU_CLOCK,	PERF_COUNT_SW_CPU_CLOCK),
		S(TPCNT_SW_TASK_CLOCK,	PERF_COUNT_SW_TASK_CLOCK),
		S(TPCNT_SW_PFLT,	PERF_COUNT_SW_PAGE_FAULTS),
		S(TPCNT_SW_CTX_SW,	PERF_COUNT_SW_CONTEXT_SWITCHES),
		S(TPCNT_SW_CPU_MIG,	PERF_COUNT_SW_CPU_MIGRATIONS),
		S(TPCNT_SW_PFLT_MIN,	PERF_COUNT_SW_PAGE_FAULTS_MIN),
		S(TPCNT_SW_PFLT_MAJ,	PERF_COUNT_SW_PAGE_FAULTS_MAJ)
	};
#undef S
	struct perf_event_attr perf_attr;

	if ((id < 0) || (id >= TPCNT_SW_NUM)){
		nm_syslog(LOG_ERR, "nmpe_open_sw: invalid id '%d'", id);
		return -1;
	}

	memset(&perf_attr, 0, sizeof(struct perf_event_attr));
	perf_attr.size = sizeof(struct perf_event_attr);
	perf_attr.type = PERF_TYPE_SOFTWARE;
	perf_attr.config = perf_ids[id];

	if (!(conf & NMPE_SW_CONF_USERSPACE))
		perf_attr.exclude_user = 1;
	if (!(conf & NMPE_SW_CONF_KERNELSPACE))
		perf_attr.exclude_kernel = 1;
	if (!(conf & NMPE_SW_CONF_HYPERVISOR))
		perf_attr.exclude_hv = 1;
	if (!(conf & NMPE_SW_CONF_IDLE))
		perf_attr.exclude_idle = 1;

	if (open_perf_events(&perf_attr, &sw_event[id])){
		close_perf_events(&sw_event[id]);
		return -1;
	}

	return 0;
}


int nmpe_open_raw(int id, uint64_t conf){
	struct perf_event_attr perf_attr;

	if ((id < 0) || (id >= CPU_PERF_NUM)){
		nm_syslog(LOG_ERR, "nmpe_open_sw: invalid id '%d'", id);
		return -1;
	}

	memset(&perf_attr, 0, sizeof(struct perf_event_attr));
	perf_attr.size = sizeof(struct perf_event_attr);
	perf_attr.type = PERF_TYPE_RAW;
	perf_attr.config = conf;

	if (open_perf_events(&perf_attr, &raw_event[id])){
		close_perf_events(&raw_event[id]);;
		return -1;
	}

	return 0;
}


static int nm_read(int fd, uint8_t *buf, size_t size){
	ssize_t rbytes;
	size_t offset = 0;

	while (offset < size){
		rbytes = read(fd, buf + offset, size - offset);
		if (rbytes == -1){
			nm_syslog(LOG_ERR, "read: %s", strerror(errno));
			return -1;
		}
		offset += rbytes;
	}

	return 0;
}


static int read_perf_events(nmpe_event_t *event, uint64_t *buf){
	int i;
	int *fds = event->fds;

	for (i = 0; i < num_cpus; i++){
		if (nm_read(fds[i], (uint8_t *)(buf + i), sizeof(uint64_t))){
			return -1;
		}
	}

	return 0;
}


int nmpe_read_sw(int id, uint64_t *buf){
	if ((id < 0) || (id >= TPCNT_SW_NUM)){
		nm_syslog(LOG_ERR, "nmpe_read_sw: invalid id '%d'", id);
		return -1;
	}

	return read_perf_events(&sw_event[id], buf);
}


int nmpe_read_raw(int id, uint64_t *buf){
	if ((id < 0) || (id >= CPU_PERF_NUM)){
		nm_syslog(LOG_ERR, "nmpe_read_raw: invalid id '%d'", id);
		return -1;
	}

	return read_perf_events(&raw_event[id], buf);
}

