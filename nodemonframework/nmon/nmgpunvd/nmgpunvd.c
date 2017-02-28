#define _GNU_SOURCE		// for ffsll(), endianness (htobe64)
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <endian.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/shm.h>

#include <nm_module.h>
#include <nm_syslog.inc.c>

#ifdef NO_LOCAL_NVML_HEADER
#include <nvml.h>
#else
#include "nvml_cuda_40.h"
#endif

#define ARRAY_SIZE(a)	(sizeof (a) / sizeof ((a)[0]))

#define _STRINGIFY(s)	#s
#define STRINGIFY(s)	_STRINGIFY(s)


/*
 * local reimplementation of htobe* for old versions
 * of glibc found in RHEL/CentOS 5.X (glibc-2.5)
 */
#if !__GLIBC_PREREQ(2,9)

#define htobe16(x)	htons(x)
#define htobe32(x)	htonl(x)
#define htobe64(x)	htonll(x)

/* straightforward implementation copied from other nmond-module */
static inline uint64_t htonll(uint64_t val){
#ifndef __BIG_ENDIAN__
	uint64_t res;
	res = (((uint64_t)htonl(val >> 32)) & 0xFFFFFFFF) |
	    (((uint64_t)(htonl(val & 0xFFFFFFFF))) << 32);
	return res;
#else
	return val;
#endif
}

#endif //__GLIBC_PREREQ




/* indicates wether we are run from monitoring framework */
static int in_framework;

/* generic logging function */
static void genlog(int prio, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);

	if (in_framework)
		nm_vsyslog(prio, fmt, args);
	else
		vfprintf(stderr, fmt, args);

	va_end(args);
}

#define err(...)	genlog(LOG_ERR, __VA_ARGS__)
#define warn(...)	genlog(LOG_WARNING, __VA_ARGS__)
#define info(...)	genlog(LOG_INFO, __VA_ARGS__)

#define gpu_log(prio, gpu, fmt, ...) \
	genlog(prio, "gpu %d, " fmt, gpu->dev_idx, ## __VA_ARGS__)

#define gpu_err(...)	gpu_log(LOG_ERR, __VA_ARGS__)
#define gpu_warn(...)	gpu_log(LOG_WARNING, __VA_ARGS__)
#define gpu_info(...)	gpu_log(LOG_INFO, __VA_ARGS__)




#ifdef NO_NVML_DEPENDENCY

/****** Dirty hack to avoid libnvidia-ml.so dependency for nmon package ******/

#include <dlfcn.h>

/* define local pointers (with "p_" prefix) to used nvml-functions */
#define P(func)		static typeof(func)	*p_ ## func

P(nvmlDeviceGetClockInfo);
P(nvmlDeviceGetCount);
P(nvmlDeviceGetDetailedEccErrors);
P(nvmlDeviceGetEccMode);
P(nvmlDeviceGetFanSpeed);
P(nvmlDeviceGetHandleByIndex);
P(nvmlDeviceGetMemoryInfo);
P(nvmlDeviceGetPowerManagementMode);
P(nvmlDeviceGetPowerUsage);
P(nvmlDeviceGetTemperature);
P(nvmlDeviceGetTotalEccErrors);
P(nvmlDeviceGetUtilizationRates);
P(nvmlInit);
P(nvmlShutdown);
#if NVML_API_VERSION >= 2
P(nvmlErrorString);
#endif

#undef P

static void *lib_handle;

static int init_nvml_syms()
{
	int i;

#define S(sym)	{.name = #sym, .dst = (void *) &p_ ## sym}

	const struct {
		char	*name;
		void	**dst;
	} nvml_syms[] = {
		S(nvmlDeviceGetClockInfo),
		S(nvmlDeviceGetCount),
		S(nvmlDeviceGetDetailedEccErrors),
		S(nvmlDeviceGetEccMode),
		S(nvmlDeviceGetFanSpeed),
		S(nvmlDeviceGetHandleByIndex),
		S(nvmlDeviceGetMemoryInfo),
		S(nvmlDeviceGetPowerManagementMode),
		S(nvmlDeviceGetPowerUsage),
		S(nvmlDeviceGetTemperature),
		S(nvmlDeviceGetTotalEccErrors),
		S(nvmlDeviceGetUtilizationRates),
		S(nvmlInit),
		S(nvmlShutdown),
#if NVML_API_VERSION >= 2
		S(nvmlErrorString),
#endif
	};
#undef S

	/* currently we can work with the major version 1 */
	lib_handle = dlopen("libnvidia-ml.so.1", RTLD_LAZY);
	if (!lib_handle) {
		err("dlopen failed, %s\n", dlerror());
		return -1;
	}

	/* find all needed symbols and set function pointers */
	for (i = 0; i < ARRAY_SIZE(nvml_syms); i++) {
		void *func;

		func = dlsym(lib_handle, nvml_syms[i].name);
		if (!func) {
			err("dlsym() call failed for \"%s\", %s\n",
						 nvml_syms[i].name, dlerror());
			return -1;
		}

		*nvml_syms[i].dst = func;
	}

	return 0;
}

static void shutdown_nvml_syms()
{
	if (lib_handle)
		dlclose(lib_handle);
}

/* change all nvml function calls to local pointers */
#define nvmlDeviceGetClockInfo			p_nvmlDeviceGetClockInfo
#define nvmlDeviceGetCount			p_nvmlDeviceGetCount
#define nvmlDeviceGetDetailedEccErrors		p_nvmlDeviceGetDetailedEccErrors
#define nvmlDeviceGetEccMode			p_nvmlDeviceGetEccMode
#define nvmlDeviceGetFanSpeed			p_nvmlDeviceGetFanSpeed
#define nvmlDeviceGetHandleByIndex		p_nvmlDeviceGetHandleByIndex
#define nvmlDeviceGetMemoryInfo			p_nvmlDeviceGetMemoryInfo
#define nvmlDeviceGetPowerManagementMode	p_nvmlDeviceGetPowerManagementMode
#define nvmlDeviceGetPowerUsage			p_nvmlDeviceGetPowerUsage
#define nvmlDeviceGetTemperature		p_nvmlDeviceGetTemperature
#define nvmlDeviceGetTotalEccErrors		p_nvmlDeviceGetTotalEccErrors
#define nvmlDeviceGetUtilizationRates		p_nvmlDeviceGetUtilizationRates
#define nvmlInit				p_nvmlInit
#define nvmlShutdown				p_nvmlShutdown
#if NVML_API_VERSION >= 2
#define nvmlErrorString				p_nvmlErrorString
#endif


#else /* NO_NVML_DEPENDENCY */

static inline int init_nvml_syms() {return 0;}
static inline void shutdown_nvml_syms() {}

#endif



/********** 	compatibility stuff for older versions of NVML 	**********/

/*
 * nvmlErrorString was added in CUDA 4.1 (NVML_API_VERSION == 2)
 *
 * CUDA 4.0 has no API version macro (shoud be 1?), so check for
 * the case of undefined NVML_API_VERSION too
 */
#if !defined(NVML_API_VERSION) || (NVML_API_VERSION < 2)
static const char *nvmlErrorString(nvmlReturn_t result)
{
	/*
	 * array of char* would be faster, but its size would depend on
	 * the NVML_ERROR_X values, e.g. NVML_ERROR_UNKNOWN is defined
	 * as 999. this is definitely not a performance critical code, so
	 * simple switch() statement should be ok.
	 */
	switch (result) {
	case NVML_SUCCESS:			return "success";
	case NVML_ERROR_UNINITIALIZED:		return "NVML uninitialized";
	case NVML_ERROR_INVALID_ARGUMENT:	return "invalid argument";
	case NVML_ERROR_NOT_SUPPORTED:		return "not supported";
	case NVML_ERROR_NO_PERMISSION:		return "no permission";
	case NVML_ERROR_ALREADY_INITIALIZED:	return "NVML already initialized";
	case NVML_ERROR_NOT_FOUND:		return "not found";
	case NVML_ERROR_INSUFFICIENT_SIZE: 	return "insufficient size";
	case NVML_ERROR_INSUFFICIENT_POWER:	return "insufficient power";
	case NVML_ERROR_UNKNOWN:		return "unknown";
	};

	return "have no idea, sorry";
}
#endif

/***********	end of compatibility stuff	**********/



/*
 * should be unsigned type, to prevent sign propagation by the
 * right shift operator (when the most significant bit is set)
 * in the for_each_set_stat_idx() macro
 */
typedef uint64_t mask_t;
#define MASK(idx)	(1ULL << (idx))


/* each stat/sensor has its own index */
enum {
	GPU_STAT_FAN_SPEED_IDX = 0,

	GPU_STAT_TEMP_GPU_IDX,

	GPU_STAT_CLOCK_GRAPHICS_IDX,
	GPU_STAT_CLOCK_SM_IDX,
	GPU_STAT_CLOCK_MEM_IDX,

	GPU_STAT_POWER_USAGE_IDX,

	GPU_STAT_MEM_FREE_IDX,
	GPU_STAT_MEM_USED_IDX,

	GPU_STAT_UTIL_GPU_IDX,
	GPU_STAT_UTIL_MEM_IDX,

	GPU_STAT_ECC_S_V_TOTAL_IDX,
	GPU_STAT_ECC_S_A_TOTAL_IDX,
	GPU_STAT_ECC_D_V_TOTAL_IDX,
	GPU_STAT_ECC_D_A_TOTAL_IDX,

	GPU_STAT_ECC_S_V_L1CACHE_IDX,
	GPU_STAT_ECC_S_V_L2CACHE_IDX,
	GPU_STAT_ECC_S_V_DEVMEM_IDX,
	GPU_STAT_ECC_S_V_REGFILE_IDX,
	GPU_STAT_ECC_S_A_L1CACHE_IDX,
	GPU_STAT_ECC_S_A_L2CACHE_IDX,
	GPU_STAT_ECC_S_A_DEVMEM_IDX,
	GPU_STAT_ECC_S_A_REGFILE_IDX,

	GPU_STAT_ECC_D_V_L1CACHE_IDX,
	GPU_STAT_ECC_D_V_L2CACHE_IDX,
	GPU_STAT_ECC_D_V_DEVMEM_IDX,
	GPU_STAT_ECC_D_V_REGFILE_IDX,
	GPU_STAT_ECC_D_A_L1CACHE_IDX,
	GPU_STAT_ECC_D_A_L2CACHE_IDX,
	GPU_STAT_ECC_D_A_DEVMEM_IDX,
	GPU_STAT_ECC_D_A_REGFILE_IDX,

	GPU_STAT_NUM
};

/* bit masks for each stat/sensor */
#define GPU_STAT_FAN_SPEED		MASK(GPU_STAT_FAN_SPEED_IDX)
#define GPU_STAT_TEMP_GPU		MASK(GPU_STAT_TEMP_GPU_IDX)
#define GPU_STAT_CLOCK_GRAPHICS		MASK(GPU_STAT_CLOCK_GRAPHICS_IDX)
#define GPU_STAT_CLOCK_SM		MASK(GPU_STAT_CLOCK_SM_IDX)
#define GPU_STAT_CLOCK_MEM		MASK(GPU_STAT_CLOCK_MEM_IDX)
#define GPU_STAT_POWER_USAGE		MASK(GPU_STAT_POWER_USAGE_IDX)
#define GPU_STAT_MEM_FREE		MASK(GPU_STAT_MEM_FREE_IDX)
#define GPU_STAT_MEM_USED		MASK(GPU_STAT_MEM_USED_IDX)
#define GPU_STAT_UTIL_GPU		MASK(GPU_STAT_UTIL_GPU_IDX)
#define GPU_STAT_UTIL_MEM		MASK(GPU_STAT_UTIL_MEM_IDX)
#define GPU_STAT_ECC_S_V_TOTAL		MASK(GPU_STAT_ECC_S_V_TOTAL_IDX)
#define GPU_STAT_ECC_S_A_TOTAL		MASK(GPU_STAT_ECC_S_A_TOTAL_IDX)
#define GPU_STAT_ECC_D_V_TOTAL		MASK(GPU_STAT_ECC_D_V_TOTAL_IDX)
#define GPU_STAT_ECC_D_A_TOTAL		MASK(GPU_STAT_ECC_D_A_TOTAL_IDX)
#define GPU_STAT_ECC_S_V_L1CACHE	MASK(GPU_STAT_ECC_S_V_L1CACHE_IDX)
#define GPU_STAT_ECC_S_V_L2CACHE	MASK(GPU_STAT_ECC_S_V_L2CACHE_IDX)
#define GPU_STAT_ECC_S_V_DEVMEM		MASK(GPU_STAT_ECC_S_V_DEVMEM_IDX)
#define GPU_STAT_ECC_S_V_REGFILE	MASK(GPU_STAT_ECC_S_V_REGFILE_IDX)
#define GPU_STAT_ECC_S_A_L1CACHE	MASK(GPU_STAT_ECC_S_A_L1CACHE_IDX)
#define GPU_STAT_ECC_S_A_L2CACHE	MASK(GPU_STAT_ECC_S_A_L2CACHE_IDX)
#define GPU_STAT_ECC_S_A_DEVMEM		MASK(GPU_STAT_ECC_S_A_DEVMEM_IDX)
#define GPU_STAT_ECC_S_A_REGFILE	MASK(GPU_STAT_ECC_S_A_REGFILE_IDX)
#define GPU_STAT_ECC_D_V_L1CACHE	MASK(GPU_STAT_ECC_D_V_L1CACHE_IDX)
#define GPU_STAT_ECC_D_V_L2CACHE	MASK(GPU_STAT_ECC_D_V_L2CACHE_IDX)
#define GPU_STAT_ECC_D_V_DEVMEM		MASK(GPU_STAT_ECC_D_V_DEVMEM_IDX)
#define GPU_STAT_ECC_D_V_REGFILE	MASK(GPU_STAT_ECC_D_V_REGFILE_IDX)
#define GPU_STAT_ECC_D_A_L1CACHE	MASK(GPU_STAT_ECC_D_A_L1CACHE_IDX)
#define GPU_STAT_ECC_D_A_L2CACHE	MASK(GPU_STAT_ECC_D_A_L2CACHE_IDX)
#define GPU_STAT_ECC_D_A_DEVMEM		MASK(GPU_STAT_ECC_D_A_DEVMEM_IDX)
#define GPU_STAT_ECC_D_A_REGFILE	MASK(GPU_STAT_ECC_D_A_REGFILE_IDX)

#define GPU_STAT_ALL			(MASK(GPU_STAT_NUM) - 1)


/* ECC per-bit-type and per-counter-type masks */
#define GPU_STAT_ECC_TOTAL_S_MASK	(GPU_STAT_ECC_S_V_TOTAL	|\
					 GPU_STAT_ECC_S_A_TOTAL	)

#define GPU_STAT_ECC_TOTAL_D_MASK	(GPU_STAT_ECC_D_V_TOTAL	|\
					 GPU_STAT_ECC_D_A_TOTAL	)

#define GPU_STAT_ECC_TOTAL_V_MASK	(GPU_STAT_ECC_S_V_TOTAL	|\
					 GPU_STAT_ECC_D_V_TOTAL	)

#define GPU_STAT_ECC_TOTAL_A_MASK	(GPU_STAT_ECC_S_A_TOTAL	|\
					 GPU_STAT_ECC_D_A_TOTAL	)

#define GPU_STAT_ECC_TOTAL_MASK		(GPU_STAT_ECC_TOTAL_S_MASK |\
					 GPU_STAT_ECC_TOTAL_D_MASK )


#define GPU_STAT_ECC_DETAILED_S_MASK 	(GPU_STAT_ECC_S_V_L1CACHE |\
					 GPU_STAT_ECC_S_V_L2CACHE |\
					 GPU_STAT_ECC_S_V_DEVMEM  |\
					 GPU_STAT_ECC_S_V_REGFILE |\
					 GPU_STAT_ECC_S_A_L1CACHE |\
					 GPU_STAT_ECC_S_A_L2CACHE |\
					 GPU_STAT_ECC_S_A_DEVMEM  |\
					 GPU_STAT_ECC_S_A_REGFILE )

#define GPU_STAT_ECC_DETAILED_D_MASK	(GPU_STAT_ECC_D_V_L1CACHE |\
					 GPU_STAT_ECC_D_V_L2CACHE |\
					 GPU_STAT_ECC_D_V_DEVMEM  |\
					 GPU_STAT_ECC_D_V_REGFILE |\
					 GPU_STAT_ECC_D_A_L1CACHE |\
					 GPU_STAT_ECC_D_A_L2CACHE |\
					 GPU_STAT_ECC_D_A_DEVMEM  |\
					 GPU_STAT_ECC_D_A_REGFILE )

#define GPU_STAT_ECC_DETAILED_V_MASK	(GPU_STAT_ECC_S_V_L1CACHE |\
					 GPU_STAT_ECC_S_V_L2CACHE |\
					 GPU_STAT_ECC_S_V_DEVMEM  |\
					 GPU_STAT_ECC_S_V_REGFILE |\
					 GPU_STAT_ECC_D_V_L1CACHE |\
					 GPU_STAT_ECC_D_V_L2CACHE |\
					 GPU_STAT_ECC_D_V_DEVMEM  |\
					 GPU_STAT_ECC_D_V_REGFILE )

#define GPU_STAT_ECC_DETAILED_A_MASK	(GPU_STAT_ECC_S_A_L1CACHE |\
					 GPU_STAT_ECC_S_A_L2CACHE |\
					 GPU_STAT_ECC_S_A_DEVMEM  |\
					 GPU_STAT_ECC_S_A_REGFILE |\
					 GPU_STAT_ECC_D_A_L1CACHE |\
					 GPU_STAT_ECC_D_A_L2CACHE |\
					 GPU_STAT_ECC_D_A_DEVMEM  |\
					 GPU_STAT_ECC_D_A_REGFILE )


#define GPU_STAT_ECC_DETAILED_MASK	(GPU_STAT_ECC_DETAILED_S_MASK |\
					 GPU_STAT_ECC_DETAILED_D_MASK )

#define GPU_STAT_ECC_AGGREGATE_MASK	(GPU_STAT_ECC_TOTAL_A_MASK    |\
					 GPU_STAT_ECC_DETAILED_A_MASK )


/*
 * Array of per-stat descriptors; some helper macros. This array
 * contains all constant information needed to handle (print to
 * terminal or send to MMCS in form of sensors) stats results.
 */

enum sens_type { SENS_T_COUNTER, SENS_T_GAUGE };

struct stat_info {
#define HDRS_NUM	3
	char		*hdrs[HDRS_NUM];
	int		sensor_id;
	enum sens_type	sensor_type;
	int		sensor_len;
	union {
		uint16_t	u16;
		uint32_t	u32;
		uint64_t	u64;
	} fail;
};

#define _S(type, bits, idx, sens_id, h1, h2, h3, fail_val)		\
	[(idx)] = {							\
		.hdrs = {(h1), (h2), (h3)},				\
		.sensor_id 	= (sens_id),				\
		.sensor_type	= (type),				\
		.sensor_len	= (bits),				\
		.fail.u ## bits	= 					\
			(typeof( 					\
				((struct stat_info *)0)->fail.u ## bits	\
			       )					\
			) (fail_val),					\
	}

/*
 * S() just expands short name to the pair of stat idx and
 * sensor id, e.g.:
 * 	XYZ -> GPU_STAT_XYZ_IDX, MON_GPU_NV_XYZ
 *
 * if these names do not match, _S() macro should be used directly
 */
#define S(type, bits, short_name, ...)					      \
		_S( type, bits, 					       \
		    GPU_STAT_ ## short_name ## _IDX, MON_GPU_NV_ ## short_name,	\
		    __VA_ARGS__)

/* imply error code (-1) for counter-sensors */
#define SC(...)		S(SENS_T_COUNTER, __VA_ARGS__, -1)
#define SG(...)		S(SENS_T_GAUGE  , __VA_ARGS__)

#define SC16(...)	SC(16, __VA_ARGS__)
#define SC32(...)	SC(32, __VA_ARGS__)
#define SC64(...)	SC(64, __VA_ARGS__)

#define SG16(...)	SG(16, __VA_ARGS__)
#define SG32(...)	SG(32, __VA_ARGS__)
#define SG64(...)	SG(64, __VA_ARGS__)

static const struct stat_info stats_info[GPU_STAT_NUM] = {
	SG16(	FAN_SPEED,		"fan","speed","% ",	-1),
	SG16(	TEMP_GPU,		"temp","gpu","C ",	-1),
	SG16(	CLOCK_GRAPHICS,		"clock","graph","MHz",	-1),
	SG16(	CLOCK_SM,		"clock","sm","MHz",	-1),
	SG16(	CLOCK_MEM,		"clock","mem","MHz",	-1),
	SG32(	POWER_USAGE,		"power","usage","mW ",	-1),
	SG64(	MEM_FREE,		"mem","free","MiB",	-1),
	SG64(	MEM_USED,		"mem","used","MiB",	-1),
	SG16(	UTIL_GPU,		"util","gpu","% ",	-1),
	SG16(	UTIL_MEM,		"util","mem","% ",	-1),
	SC64(	ECC_S_V_TOTAL,		"ecc","S-V","tot"),
	SC64(	ECC_S_A_TOTAL,		"ecc","S-A","tot"),
	SC64(	ECC_D_V_TOTAL,		"ecc","D-V","tot"),
	SC64(	ECC_D_A_TOTAL,		"ecc","D-A","tot"),
	SC32(	ECC_S_V_L1CACHE,	"ecc","S-V","l1"),
	SC32(	ECC_S_V_L2CACHE,	"ecc","S-V","l2"),
	SC32(	ECC_S_V_DEVMEM,		"ecc","S-V","mem"),
	SC32(	ECC_S_V_REGFILE,	"ecc","S-V","reg"),
	SC32(	ECC_S_A_L1CACHE,	"ecc","S-A","l1"),
	SC32(	ECC_S_A_L2CACHE,	"ecc","S-A","l2"),
	SC32(	ECC_S_A_DEVMEM,		"ecc","S-A","mem"),
	SC32(	ECC_S_A_REGFILE,	"ecc","S-A","reg"),
	SC32(	ECC_D_V_L1CACHE,	"ecc","D-V","l1"),
	SC32(	ECC_D_V_L2CACHE,	"ecc","D-V","l2"),
	SC32(	ECC_D_V_DEVMEM,		"ecc","D-V","mem"),
	SC32(	ECC_D_V_REGFILE,	"ecc","D-V","reg"),
	SC32(	ECC_D_A_L1CACHE,	"ecc","D-A","l1"),
	SC32(	ECC_D_A_L2CACHE,	"ecc","D-A","l2"),
	SC32(	ECC_D_A_DEVMEM,		"ecc","D-A","mem"),
	SC32(	ECC_D_A_REGFILE,	"ecc","D-A","reg"),
};
#undef _S
#undef S



/* this structure represents results of one iteration of stats reading */
struct gpu_stats {
	struct gpu		*gpu;

	/* bitmask of successfully acquired stats */
	mask_t			mask;

	/* for now all stats are representable in ull, so keep it simple */
	unsigned long long 	values[GPU_STAT_NUM];
};


/* GPU device internal representaion */
struct gpu {
	int		flags;
#define GPU_F_ACTIVE		(1 << 0)	// if unset - ignore device
#define GPU_F_PM_CHECKED	(1 << 1)	// no need to check again
#define GPU_F_PM_OK		(1 << 2)	// if set - PM is available
#define GPU_F_ECC_CHECKED	(1 << 3)
#define GPU_F_ECC_OK		(1 << 4)

	mask_t		mask;			// supported stats
	nvmlDevice_t	dev;
	int		dev_idx;

	struct gpu_stats stats;
};



/* NVML request descriptor */
struct gpu_stat_request {
	mask_t	mask;		// mask of stats handled by this NVML request
	int (*request)(mask_t, struct gpu_stats *);
};


/* request funcs forward declarations needed for following array definition */
static int nv_get_clock(mask_t, struct gpu_stats *);
static int nv_get_ecc_detailed (mask_t, struct gpu_stats *);
static int nv_get_fan_speed (mask_t, struct gpu_stats *);
static int nv_get_memory_info (mask_t, struct gpu_stats *);
static int nv_get_power_usage (mask_t, struct gpu_stats *);
static int nv_get_temp (mask_t, struct gpu_stats *);
static int nv_get_ecc_total (mask_t, struct gpu_stats *);
static int nv_get_utilization (mask_t, struct gpu_stats *);


#define R(f, m)	{.request = (f), .mask = (m)}

static const struct gpu_stat_request gpu_stat_requests[] = {
	R(nv_get_clock, 	GPU_STAT_CLOCK_GRAPHICS |
				GPU_STAT_CLOCK_SM 	|
				GPU_STAT_CLOCK_MEM),

	R(nv_get_ecc_detailed,  GPU_STAT_ECC_DETAILED_MASK),

	R(nv_get_fan_speed,	GPU_STAT_FAN_SPEED),

	R(nv_get_memory_info,	GPU_STAT_MEM_FREE |
				GPU_STAT_MEM_USED),

	R(nv_get_power_usage,	GPU_STAT_POWER_USAGE),

	R(nv_get_temp,		GPU_STAT_TEMP_GPU),

	R(nv_get_ecc_total,	GPU_STAT_ECC_TOTAL_MASK),

	R(nv_get_utilization, 	GPU_STAT_UTIL_GPU |
				GPU_STAT_UTIL_MEM),
};
#undef R

/*
 * macro for using as a for() statement in cycles executing some
 * code for every stat idx (assigned to the first parameter i)
 * set in the mask (passed as the second parameter)
 *
 * mask is evaluated several times, so don't pass expressions
 * with side effects
 */
#define for_each_set_stat_idx(i, mask)				      \
		for (i = 0;					       \
		      ((mask) & GPU_STAT_ALL) >> i && 			\
		      (i += ffsll(((mask) & GPU_STAT_ALL) >> i) - 1 , 1);\
		     i++)


/* mask here should contain only one non-zero bit */
static void stats_add(struct gpu_stats *stats, mask_t mask,
						unsigned long long value)
{
	int idx = ffsll(mask) - 1;

	/* should never happen, just in case */
	if (idx < 0 || idx >= GPU_STAT_NUM)
		gpu_err(stats->gpu, "suspicious stat index %d mask 0x%08llx "
			"GPU_STAT_NUM %d\n", idx, (unsigned long long) mask,
								GPU_STAT_NUM);

	stats->values[idx] = value;
	stats->mask |= mask;
}


/***************	 wrappers for NVML requests		***************/

static int nv_get_clock(mask_t mask, struct gpu_stats *stats)
{
	int i;
	/*
	 * convert stat mask to nvml clock type with two const arrays
	 *		m[0] => ct[0]
	 *		m[1] => ct[1]
	 *		    ...
	 */
	const mask_t m[] = { GPU_STAT_CLOCK_GRAPHICS,
			     GPU_STAT_CLOCK_SM,
			     GPU_STAT_CLOCK_MEM };

	const nvmlClockType_t ct[] = { NVML_CLOCK_GRAPHICS,
				       NVML_CLOCK_SM,
				       NVML_CLOCK_MEM };

	for (i = 0; i < ARRAY_SIZE(m); i++) {
		nvmlReturn_t nret;
		unsigned int clk;

		if (!(mask & m[i]))
			continue;

		nret = nvmlDeviceGetClockInfo(stats->gpu->dev, ct[i], &clk);
		if (nret != NVML_SUCCESS) {
			gpu_err(stats->gpu, "failed clockinfo (%d) call: %s\n",
						  ct[i], nvmlErrorString(nret));
			continue;
		}

		stats_add(stats, m[i], clk);
	}

	return 0;
}

static int nv_get_fan_speed(mask_t mask, struct gpu_stats *stats)
{
	unsigned int speed;
	nvmlReturn_t nret;

	nret = nvmlDeviceGetFanSpeed(stats->gpu->dev, &speed);
	if (nret != NVML_SUCCESS) {
		gpu_err(stats->gpu, "failed fan speed call: %s\n",
						nvmlErrorString(nret));
		return -1;
	}

	stats_add(stats, GPU_STAT_FAN_SPEED, speed);

	return 0;
}


static int nv_get_memory_info(mask_t mask, struct gpu_stats *stats)
{
	nvmlReturn_t nret;
	nvmlMemory_t mem;

	nret = nvmlDeviceGetMemoryInfo(stats->gpu->dev, &mem);
	if (nret != NVML_SUCCESS) {
		gpu_err(stats->gpu, "failed memory info call: %s\n",
						nvmlErrorString(nret));
		return -1;
	}

	if (mask & GPU_STAT_MEM_FREE)
		stats_add(stats, GPU_STAT_MEM_FREE, mem.free);

	if (mask & GPU_STAT_MEM_USED)
		stats_add(stats, GPU_STAT_MEM_USED, mem.used);

	return 0;
}


static int pmm_enabled(struct gpu *gpu)
{
	nvmlEnableState_t mode;
	nvmlReturn_t nret;

	/* if it's already checked, return the cached value */
	if (gpu->flags & GPU_F_PM_CHECKED)
		return !!(gpu->flags & GPU_F_PM_OK);

	/* get and cache Power Management Mode status */

	gpu->flags |= GPU_F_PM_CHECKED;
	gpu->flags &= ~GPU_F_PM_OK;

	nret = nvmlDeviceGetPowerManagementMode(gpu->dev, &mode);
	if (nret != NVML_SUCCESS) {
		gpu_err(gpu, "failed power mode call: %s\n",
						nvmlErrorString(nret));
		return 0;
	}

	if (mode == NVML_FEATURE_DISABLED) {
		gpu_err(gpu, "power management is disabled\n");
		return 0;
	}

	gpu->flags |= GPU_F_PM_OK;

	return 1;
}


static int nv_get_power_usage(mask_t mask, struct gpu_stats *stats)
{
	unsigned int power;
	nvmlReturn_t nret;

	if (!pmm_enabled(stats->gpu))
		return -1;

	nret = nvmlDeviceGetPowerUsage(stats->gpu->dev, &power);
	if (nret != NVML_SUCCESS) {
		gpu_err(stats->gpu, "failed power usage call: %s\n",
						nvmlErrorString(nret));
		return -1;
	}

	stats_add(stats, GPU_STAT_POWER_USAGE, power);

	return 0;
}


static int nv_get_temp(mask_t mask, struct gpu_stats *stats)
{
	nvmlReturn_t nret;
	unsigned int temp;

	/* currently only GPU die sensor type is supported, so keep it simple */

	nret = nvmlDeviceGetTemperature(stats->gpu->dev, NVML_TEMPERATURE_GPU,
									&temp);
	if (nret != NVML_SUCCESS) {
		gpu_err(stats->gpu, "failed temperature call: %s\n",
						nvmlErrorString(nret));
		return -1;
	}

	stats_add(stats, GPU_STAT_TEMP_GPU, temp);

	return 0;
}


static int nv_get_utilization(mask_t mask, struct gpu_stats *stats)
{
	nvmlUtilization_t util;
	nvmlReturn_t nret;

	nret = nvmlDeviceGetUtilizationRates(stats->gpu->dev, &util);
	if (nret != NVML_SUCCESS) {
		gpu_err(stats->gpu, "failed utilization call: %s\n",
						nvmlErrorString(nret));
		return -1;
	}

	if (mask & GPU_STAT_UTIL_GPU)
		stats_add(stats, GPU_STAT_UTIL_GPU, util.gpu);

	if (mask & GPU_STAT_UTIL_MEM)
		stats_add(stats, GPU_STAT_UTIL_MEM, util.memory);

	return 0;
}


/****************	ECC conversion/lookup tables	***************/

enum {
	ECC_BIT_SINGLE = 0,
	ECC_BIT_DOUBLE,

	ECC_BIT_NUM
};

enum {
	ECC_CNT_VOLATILE = 0,
	ECC_CNT_AGGREGATE,

	ECC_CNT_NUM
};

/* locations of detailed ECC errors */
enum {
	ECC_LOC_L1CACHE = 0,
	ECC_LOC_L2CACHE,
	ECC_LOC_DEVMEM,
	ECC_LOC_REGFILE,

	ECC_LOC_NUM
};

/* used to determine stat id from bit/counter/location types for detailed ECC  */
static const mask_t ecc_map_detailed[ECC_BIT_NUM][ECC_CNT_NUM][ECC_LOC_NUM] = {

	[ECC_BIT_SINGLE] = {

		[ECC_CNT_VOLATILE] = {
			[ECC_LOC_L1CACHE] = GPU_STAT_ECC_S_V_L1CACHE,
			[ECC_LOC_L2CACHE] = GPU_STAT_ECC_S_V_L2CACHE,
			[ECC_LOC_DEVMEM]  = GPU_STAT_ECC_S_V_DEVMEM,
			[ECC_LOC_REGFILE] = GPU_STAT_ECC_S_V_REGFILE,
		},

		[ECC_CNT_AGGREGATE] = {
			[ECC_LOC_L1CACHE] = GPU_STAT_ECC_S_A_L1CACHE,
			[ECC_LOC_L2CACHE] = GPU_STAT_ECC_S_A_L2CACHE,
			[ECC_LOC_DEVMEM]  = GPU_STAT_ECC_S_A_DEVMEM,
			[ECC_LOC_REGFILE] = GPU_STAT_ECC_S_A_REGFILE,
		},
	},

	[ECC_BIT_DOUBLE] = {

		[ECC_CNT_VOLATILE] = {
			[ECC_LOC_L1CACHE] = GPU_STAT_ECC_D_V_L1CACHE,
			[ECC_LOC_L2CACHE] = GPU_STAT_ECC_D_V_L2CACHE,
			[ECC_LOC_DEVMEM]  = GPU_STAT_ECC_D_V_DEVMEM,
			[ECC_LOC_REGFILE] = GPU_STAT_ECC_D_V_REGFILE,
		},

		[ECC_CNT_AGGREGATE] = {
			[ECC_LOC_L1CACHE] = GPU_STAT_ECC_D_A_L1CACHE,
			[ECC_LOC_L2CACHE] = GPU_STAT_ECC_D_A_L2CACHE,
			[ECC_LOC_DEVMEM]  = GPU_STAT_ECC_D_A_DEVMEM,
			[ECC_LOC_REGFILE] = GPU_STAT_ECC_D_A_REGFILE,
		},

	}
};

/* used to determine stat id from bit/counter types for total ECC  */
static const mask_t ecc_map_total[ECC_BIT_NUM][ECC_CNT_NUM] = {

	[ECC_BIT_SINGLE] = {
		[ECC_CNT_VOLATILE]  = GPU_STAT_ECC_S_V_TOTAL,
		[ECC_CNT_AGGREGATE] = GPU_STAT_ECC_S_A_TOTAL,
	},

	[ECC_BIT_DOUBLE] = {
		[ECC_CNT_VOLATILE]  = GPU_STAT_ECC_D_V_TOTAL,
		[ECC_CNT_AGGREGATE] = GPU_STAT_ECC_D_A_TOTAL,
	}
};


/* full stats masks for each bit type and counter type */
static const mask_t ecc_bit_mask_total[ECC_BIT_NUM] = {
	[ECC_BIT_SINGLE] = GPU_STAT_ECC_TOTAL_S_MASK,
	[ECC_BIT_DOUBLE] = GPU_STAT_ECC_TOTAL_D_MASK,
};

static const mask_t ecc_bit_mask_detailed[ECC_BIT_NUM] = {
	[ECC_BIT_SINGLE] = GPU_STAT_ECC_DETAILED_S_MASK,
	[ECC_BIT_DOUBLE] = GPU_STAT_ECC_DETAILED_D_MASK,
};

static const mask_t ecc_cnt_mask_total[ECC_CNT_NUM] = {
	[ECC_CNT_VOLATILE]  = GPU_STAT_ECC_TOTAL_V_MASK,
	[ECC_CNT_AGGREGATE] = GPU_STAT_ECC_TOTAL_A_MASK,
};

static const mask_t ecc_cnt_mask_detailed[ECC_CNT_NUM] = {
	[ECC_CNT_VOLATILE]  = GPU_STAT_ECC_DETAILED_V_MASK,
	[ECC_CNT_AGGREGATE] = GPU_STAT_ECC_DETAILED_A_MASK,
};


/* arrays for convertion to NVML types */
static const nvmlEccBitType_t ecc_map_nv_bit[ECC_BIT_NUM] = {
	[ECC_BIT_SINGLE] = NVML_SINGLE_BIT_ECC,
	[ECC_BIT_DOUBLE] = NVML_DOUBLE_BIT_ECC,
};

static const nvmlEccCounterType_t ecc_map_nv_cnt[ECC_CNT_NUM] = {
	[ECC_CNT_VOLATILE]  = NVML_VOLATILE_ECC,
	[ECC_CNT_AGGREGATE] = NVML_AGGREGATE_ECC,
};


/*****************	Generic ECC errors code		****************/

static int ecc_enabled(struct gpu *gpu)
{
	nvmlEnableState_t current, pending;
	nvmlReturn_t nret;

	/* if it's already checked, return the cached value */
	if (gpu->flags & GPU_F_ECC_CHECKED)
		return !!(gpu->flags & GPU_F_ECC_OK);

	/* get and cache ECC mode */

	gpu->flags |= GPU_F_ECC_CHECKED;
	gpu->flags &= ~GPU_F_ECC_OK;

	nret = nvmlDeviceGetEccMode(gpu->dev, &current, &pending);
	if (nret != NVML_SUCCESS) {
		gpu_err(gpu, "failed ECC mode call: %s\n",
						nvmlErrorString(nret));
		return 0;
	}

	if (current == NVML_FEATURE_DISABLED) {
		gpu_err(gpu, "ECC mode is disabled\n");
		return 0;
	}

	gpu->flags |= GPU_F_ECC_OK;

	return 1;
}


/*
 * iterate through all bit and counter types combinations, checking the mask
 * each time, if request should be made - call the provided handler function
 */
static int ecc_iterate(mask_t mask, struct gpu_stats *stats,
			int (*func)(mask_t, struct gpu_stats *, int, int),
			const mask_t ecc_bit_masks[], const mask_t ecc_cnt_masks[])
{
	int i,j;

	if (!ecc_enabled(stats->gpu))
		return -1;

	for (i = 0; i < ECC_BIT_NUM; i++)
		if (mask & ecc_bit_masks[i])
			for (j = 0; j < ECC_CNT_NUM; j++)
				if (mask & ecc_cnt_masks[j])
					func(mask, stats, i, j);
	return 0;
}



/*****************	Total ECC errors specific code	****************/

static int ecc_total_make_request(mask_t mask, struct gpu_stats *stats,
							int bit, int cnt)
{
	unsigned long long val;
	nvmlReturn_t nret;

	nret = nvmlDeviceGetTotalEccErrors(stats->gpu->dev,
		    ecc_map_nv_bit[bit], ecc_map_nv_cnt[cnt], &val);
	if (nret != NVML_SUCCESS) {
		gpu_err(stats->gpu, "failed ecc total (%d,%d) call: %s\n",
				    ecc_map_nv_bit[bit], ecc_map_nv_cnt[cnt],
							nvmlErrorString(nret));
		return -1;
	}

	stats_add(stats, ecc_map_total[bit][cnt], val);

	return 0;
}


static int nv_get_ecc_total(mask_t mask, struct gpu_stats *stats)
{
	return ecc_iterate(mask, stats, ecc_total_make_request,
				ecc_bit_mask_total, ecc_cnt_mask_total);
}


/*****************	Detailed ECC errors specific code ****************/

static int ecc_detailed_make_request(mask_t mask, struct gpu_stats *stats,
							  int bit, int cnt)
{
	nvmlEccErrorCounts_t errors;
	nvmlReturn_t nret;
	const mask_t *locs;

	nret = nvmlDeviceGetDetailedEccErrors(stats->gpu->dev,
		    ecc_map_nv_bit[bit], ecc_map_nv_cnt[cnt], &errors);
	if (nret != NVML_SUCCESS) {
		gpu_err(stats->gpu, "failed ecc detailed (%d,%d) call: %s\n",
				    ecc_map_nv_bit[bit], ecc_map_nv_cnt[cnt],
							nvmlErrorString(nret));
		return -1;
	}

	/* get reference to "location => stat id" mapping for this bit/cnt */
	locs = ecc_map_detailed[bit][cnt];

	if (mask & locs[ECC_LOC_L1CACHE])
		stats_add(stats, locs[ECC_LOC_L1CACHE], errors.l1Cache);

	if (mask & locs[ECC_LOC_L2CACHE])
		stats_add(stats, locs[ECC_LOC_L2CACHE], errors.l2Cache);

	if (mask & locs[ECC_LOC_DEVMEM])
		stats_add(stats, locs[ECC_LOC_DEVMEM], errors.deviceMemory);

	if (mask & locs[ECC_LOC_REGFILE])
		stats_add(stats, locs[ECC_LOC_REGFILE], errors.registerFile);

	return 0;
}

static int nv_get_ecc_detailed(mask_t mask, struct gpu_stats *stats)
{
	return ecc_iterate(mask, stats, ecc_detailed_make_request,
				ecc_bit_mask_detailed, ecc_cnt_mask_detailed);
}




/*
 * This function is the main entry point for the whole "reading statistics
 * from the NVML" code. It traverses the list of nvml requests, making
 * the ones needed to get stats indicated in the mask.
 */
static int get_gpu_stats(mask_t mask, struct gpu_stats *stats)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(gpu_stat_requests); i++) {
		const struct gpu_stat_request *r = &gpu_stat_requests[i];

		if (!(r->mask & mask))
			continue;

		/* ignore return value for now */
		r->request(r->mask & mask, stats);
	}

	return 0;
}



/******************	 printing table to stdout 	********************/

/* width of the longest header in each column */
static int hdr_w[GPU_STAT_NUM];

static int print_table_init(struct gpu *gpus, int nr_gpus, mask_t aggr_mask)
{
	int i, j;

	/* calculate the width of the table header strings */
	for_each_set_stat_idx(i, aggr_mask)
		for (j = 0; j < HDRS_NUM; j++) {
			int l = strlen(stats_info[i].hdrs[j]);

			if (l > hdr_w[i])
				hdr_w[i] = l;
		}

	return 0;
}


#define NA_STRING	"-"

static void print_table(struct gpu *gpus, int nr_gpus, mask_t aggr_mask)
{
	int col_w[GPU_STAT_NUM] = {0};
	int i, j, l;

	static const char *num_col[HDRS_NUM] = {"","GPU",""};
	static const int num_len = 3;


	/* calculate width of each column */
	for_each_set_stat_idx(i, aggr_mask) {
		mask_t	imask = MASK(i);

		col_w[i] = hdr_w[i];

		/* find the widest data string */
		for (j = 0; j < nr_gpus; j++) {
			unsigned long long v;

			if (!(gpus[j].flags & GPU_F_ACTIVE))
				continue;

			v = gpus[j].stats.values[i];

			/* memory info is printed in MiB instead of bytes */
			if (imask & (GPU_STAT_MEM_FREE | GPU_STAT_MEM_USED))
				v >>= 20;

			if (gpus[j].mask & imask)
				l = snprintf(NULL, 0, "%llu", v);
			else
				l = strlen(NA_STRING); // "-"

			if (l > col_w[i])
				col_w[i] = l;
		}
	}

	/* print header lines */
	for (i = 0; i < HDRS_NUM; i++) {
		printf("%*s |", num_len, num_col[i]);

		for_each_set_stat_idx(j, aggr_mask)
			printf(" %*s", col_w[j], stats_info[j].hdrs[i]);
		putchar('\n');

	}

	/* print "----+---------------" line */
	for (i = 0; i < num_len + 1; i++)
		putchar('-');
	putchar('+');

	for_each_set_stat_idx(i, aggr_mask) {
		for (j = 0; j < col_w[i]; j++)
			putchar('-');
		putchar('-');
	}

	putchar('\n');

	/* print per-GPU lines */
	for (i = 0; i < nr_gpus; i++) {
		printf("%*d |", num_len, i);

		for_each_set_stat_idx(j, aggr_mask) {
			mask_t jmask = MASK(j);
			unsigned long long v;

			/*
			 * if it's a problematic GPU or there is no such stat
			 * for this GPU - print N/A string
			 */
			if (!(gpus[i].flags & GPU_F_ACTIVE) ||
			    !(gpus[i].stats.mask & jmask))
			{
				printf(" %*s", col_w[j], NA_STRING);
				continue;
			}

			v = gpus[i].stats.values[j];

			/* print memory info in MiB instead of bytes */
			if (jmask & (GPU_STAT_MEM_FREE | GPU_STAT_MEM_USED))
				v >>= 20;

			printf(" %*llu", col_w[j], v);
		}

		putchar('\n');
	}

	putchar('\n');
}



/*****************	handling stop request on SIGTERM 	************/
static int stop_requested;
static void stop_updates(int sig) { stop_requested = 1; }
static int should_stop(void)
{
	if (in_framework)
		return stop_requested;
	return 0;
}

/**************    stuff related to dealing with MMCS-sensors  **************/

static struct nm_module_bufdesc_t *sbuf_desc;
/* sensors buffer shared with the parent nmond */
static int sbuf_shmid;
static void *sbuf;
/* each stat's sensor offset in the shared buffer */
static int stat_offset[GPU_STAT_NUM];

static void set_sensor_fail_idx(int stat_idx, int gpu_idx, struct gpu *gpu)
{
	const struct stat_info *si = &stats_info[stat_idx];
	char *sens_ptr;

	sens_ptr = sbuf + stat_offset[stat_idx] + sizeof(struct nm_tlv_hdr_t);
	sens_ptr += gpu_idx * (si->sensor_len >> 3);

	/*
	 * The only error supported for sensors of SENS_T_COUNTER type is (-1).
	 * For the sake of code simplicity it's stored (just as for "gauge"
	 * sensors) in the apporpriate member of the "fail" union in stat_info
	 * descriptor. This way we can totally avoid checking the type of the
	 * sensor here.
	 *
	 * More complex logic (with real si->sensor_type checking) should be
	 * implemented here in order to support other error codes for "counter"
	 * sensors.
	 */
	switch(si->sensor_len) {
	case 16:
		*((uint16_t*)sens_ptr) = htobe16(si->fail.u16);
		break;
	case 32:
		*((uint32_t*)sens_ptr) = htobe32(si->fail.u32);
		break;
	case 64:
		*((uint64_t*)sens_ptr) = htobe64(si->fail.u64);
		break;
	}
}


static void set_sensor_idx(int stat_idx, int gpu_idx, struct gpu *gpu,
							unsigned long long val)
{
	const struct stat_info *si = &stats_info[stat_idx];
	mask_t mask = MASK(stat_idx);
	char *sens_ptr;

	sens_ptr = sbuf + stat_offset[stat_idx] + sizeof(struct nm_tlv_hdr_t);
	sens_ptr += gpu_idx * (si->sensor_len >> 3);

	/*
	 * Below, all "NVML stat -> MMCS sensor" value transformations (e.g.
	 * bytes to KiB, overflowing, etc) are performed.
	 */

	/* all percents and degrees are sent in a centi-* form (1/100s) */
	if (mask & (GPU_STAT_FAN_SPEED | GPU_STAT_TEMP_GPU |
		    GPU_STAT_UTIL_GPU  | GPU_STAT_UTIL_MEM))
		val *= 100;

	/*
	 * The most significant bit in counter-sensors is used to indicate
	 * an error. We could set the NM_BUF_SHMID_CHANGED flag in the shared
	 * buffer descriptor wich should cause nmond to "reset" the stream.
	 * But, from what I was told, MMCS will treat such event as an agent
	 * restart which could be troublesome.
	 *
	 * Therefore, just overflow the value on (N-1) bits and hope that
	 * MMCS can deal with it (at least in the future). BTW, reset of the
	 * most significant bit of successfully handled "counter"-sensors to
	 * 0 was explicitly requested in the bug #2773.
	 */
	if (si->sensor_type == SENS_T_COUNTER)
		val &= (1ULL << (si->sensor_len - 1)) - 1;


	switch(si->sensor_len) {
	case 16:
		*((uint16_t*)sens_ptr) = htobe16(val);
		break;
	case 32:
		*((uint32_t*)sens_ptr) = htobe32(val);
		break;
	case 64:
		*((uint64_t*)sens_ptr) = htobe64(val);
		break;
	}
}

static int update_sensors(struct gpu *gpus, int nr_gpus, mask_t aggr_mask)
{
	int i, j;

	for_each_set_stat_idx (i, aggr_mask) {
		mask_t imask = MASK(i);

		for (j = 0; j < nr_gpus; j++) {
			struct gpu *gpu = &gpus[j];

			/* if information is absent, set the "failed" value */
			if (!(gpu->flags & GPU_F_ACTIVE) ||
			    !(gpu->stats.mask & imask))
			{
				set_sensor_fail_idx(i, j, gpu);
				continue;
			}

			/* everything seems ok, update sensor value */
			set_sensor_idx(i, j, gpu, gpu->stats.values[i]);
		}
	}

	return 0;
}


static int update_sensors_init(struct gpu *gpus, int nr_gpus, mask_t aggr_mask)
{
	struct sigaction term_sa = {.sa_handler = stop_updates};
	int i, ret = 0, blen = 0;

	/* calculate the buffer size, set sensors offsets */
	for_each_set_stat_idx (i, aggr_mask) {
		stat_offset[i] = blen;
		blen += NM_DYNELEMLEN(stats_info[i].sensor_len >> 3, nr_gpus);
	}

	/* allocate and attach sensors shared buffer */

	sbuf_shmid = shmget(IPC_PRIVATE, blen, S_IRUSR | S_IWUSR);
	if (sbuf_shmid == -1) {
		err("failed to create shared sensors buffer: %s\n",
							strerror(errno));
		return -1;
	}

	sbuf = shmat(sbuf_shmid, NULL, 0);
	if (sbuf == (void*) -1) {
		err("failed to attach shared sensors buffer: %s\n",
							strerror(errno));
		return -1;
	}

	ret = shmctl(sbuf_shmid, IPC_RMID, NULL);
	if (ret == -1)
		warn("failed marking sbuf to be removed: %s\n",
							strerror(errno));

	/* initialize sensor type-length pairs */
	for_each_set_stat_idx (i, aggr_mask) {
		const struct stat_info *si = &stats_info[i];
		struct nm_tlv_hdr_t *h;

		h = (typeof(h)) (sbuf + stat_offset[i]);
		*h = NM_MONTYPE(si->sensor_id, (si->sensor_len >> 3) * nr_gpus);
	}


	/* update sensor buffer descriptor (provided by nmond) */
	sbuf_desc->buf_shmid = sbuf_shmid;
	sbuf_desc->buf_size = blen;

	update_sensors(gpus, nr_gpus, aggr_mask);
	sbuf_desc->buf_flags |= NM_BUF_SHMID_CHANGED;

	/* signal handling, copied from other nmond-modules */
	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, SIG_IGN);

	sigemptyset(&term_sa.sa_mask);
	sigaction(SIGTERM, &term_sa, NULL);

	return 0;
}

static void update_sensors_exit(void)
{
	/* detach shared memory, reset fields in sbuf descriptor */

	shmdt(sbuf);

	sbuf_desc->buf_shmid = -1;
	sbuf_desc->buf_size = 0;
	sbuf_desc->buf_flags |= NM_BUF_SHMID_CHANGED;
}





/***************** 	cmdline and config file parsing	*****************/

static void print_help(void)
{
	/* print help only to stderr, it's useless to send it to nm_syslog */
	fprintf(stderr, "Usage: %s [OPTION]...\n\n"
	     "Supported options:\n"
		"\t-h            : print this message\n"
		"\t-a            : try to get all stats, ignore nothing\n"
		"\t-s key        : module SHM key\n"
		"\t-c config     : config file\n"
		"\t-i interval   : update interval in seconds\n"
		"\n",
		APPNAME);
}


struct cfg_opts {
	int	update_interval;
	int	try_all_stats;
	mask_t	ignore_mask;
	char	*config_file;
	int	shmid;
};

static struct cfg_opts cfg = {
	.update_interval = -1,	// -1 indicates that it wasn't set from cmdline
	.shmid = -1,
#ifdef CONFIGFILE
	.config_file = CONFIGFILE,
#endif
};


static int parse_cmdline(int argc, char **argv, struct cfg_opts *cfg)
{
	int opt;

	while ((opt = getopt(argc, argv, "s:c:i:ha")) != -1)
		switch (opt) {
		case 'h':
			print_help();
			exit(EXIT_SUCCESS);

		case 'c':
			cfg->config_file = strdup(optarg);
			if (!cfg->config_file) {
				err("allocation for cfg filename failed\n");
				return -1;
			}
			break;

		case 'i':
			cfg->update_interval = atoi(optarg);
			if (cfg->update_interval <= 0) {
				err("wrong interval value \"%s\"\n", optarg);
				return -1;
			}
			break;

		case 'a':
			cfg->try_all_stats = 1;
			break;

		case 's':
			cfg->shmid = atoi(optarg);
			break;

		default:
			return -1;
		}

	return 0;
}



#define CFG_FILE_OPT_NAME_MAX	30
#define CFG_FILE_OPT_VAL_MAX	30

static int parse_config(int argc, char **argv, struct cfg_opts *cfg)
{
	char line[2 * (CFG_FILE_OPT_NAME_MAX + CFG_FILE_OPT_VAL_MAX)];
	char opt[CFG_FILE_OPT_NAME_MAX + 1];
	char val[CFG_FILE_OPT_VAL_MAX + 1];
	int c, ret = 0;
	FILE *cf;

	if(!cfg->config_file) {
		info("no config file provided\n");
		return 0;
	}

	cf = fopen(cfg->config_file, "r");
	if (!cf) {
		err("failed to open config file \"%s\": %s\n",
				cfg->config_file, strerror(errno));
		return -1;
	}

	while (fgets(line, sizeof(line), cf)) {
		char ch;

		/* if the first non-white-space char is '#', it's a comment */
		c = sscanf(line, " %1c", &ch);
		if (c == 1 && ch == '#')
			continue;

		c = sscanf(line, "%" STRINGIFY(CFG_FILE_OPT_NAME_MAX) "s "
				 "%" STRINGIFY(CFG_FILE_OPT_VAL_MAX) "s",
				 opt, val);
		switch (c) {
		case 1:
			if (!strcmp(opt, "no_ecc_detailed")) {
				cfg->ignore_mask |= GPU_STAT_ECC_DETAILED_MASK;
				break;
			} else if (!strcmp(opt, "no_ecc_aggregate")) {
				cfg->ignore_mask |= GPU_STAT_ECC_AGGREGATE_MASK;
				break;
			}

			err("unknown cfg file option \"%s\"\n", opt);
			ret = -1;
			break;

		case 2:
			if (!strcmp(opt, "interval")) {
				if (cfg->update_interval == -1)
					cfg->update_interval = atoi(val);
				break;
			}

			err("unknown cfg file option \"%s\"\n", opt);
			ret = -1;
			break;

		case EOF:
			break;

		default:
			err("error parsing cfg file line \"%s\"\n", line);
			ret = -1;
			break;
		}
	}

	fclose(cf);
	return ret;
}




int main(int argc, char **argv)
{
	mask_t aggr_mask = 0;
	unsigned int nr_gpus;
	nvmlReturn_t nret;
	struct gpu *gpus;
	int i, ret = 0;
	int opt;

	/*
	 * Determine whether we are started from framework by presence
	 * of "-s" (shmid) option in the cmdline arguments.
	 *
	 * The simplest way to do this is by using getopt, but with error
	 * printing (opterr) and argv rearrangement (first '+' in optstring)
	 * disabled.
	 */
	opterr = 0;

	while ((opt = getopt(argc, argv, "+s:")) != -1)
		if (opt == 's') {
			in_framework = 1;
			break;
		}

	opterr = 1;
	optind = 1;


	/* parse command line args & config file, check options, etc */

	if (parse_cmdline(argc, argv, &cfg)) {
		err("failed to parse cmdline arguments.\n");
		return 1;
	}

	if (parse_config(argc, argv, &cfg)) {
		err("failed to parse config file.\n");
		return 2;
	}

	if (cfg.try_all_stats)
		cfg.ignore_mask = 0;

	if (cfg.update_interval <= 0)
		cfg.update_interval = NM_MON_PERIOD_SEC;


	/* attach shared memory with our buffer descriptor */
	if (in_framework && cfg.shmid != -1) {
		sbuf_desc = shmat(cfg.shmid, NULL, 0);
		if (sbuf_desc == (void*) -1) {
			err("failed to attach bufdesc shm: %s\n",
							strerror(errno));
			return 3;
		}
	}


	/* load NVML .so file if needed */
	if (init_nvml_syms()) {
		err("error while dynamically loading NVML\n");
		ret = 4;
		goto shm_dt;
	}


	/* init nvml, get GPUs count and handlers, scan for stats, etc */

	nret = nvmlInit();
	if (nret != NVML_SUCCESS) {
		err("failed to initialize NVML: %s\n", nvmlErrorString(nret));
		ret = 5;
		goto syms_shut;
	}

	nret = nvmlDeviceGetCount(&nr_gpus);
	if (nret != NVML_SUCCESS) {
		err("failed to get GPUs number: %s\n", nvmlErrorString(nret));
		ret = 6;
		goto nvml_shut;
	}

	gpus = malloc(nr_gpus * sizeof(*gpus));
	if (!gpus) {
		err("gpu descriptors memory allocation failure\n");
		ret = 7;
		goto nvml_shut;
	}
	memset(gpus, 0, nr_gpus * sizeof(*gpus));


	/* initialize gpu descriptors and get first stats reading */
	for (i = 0; i < nr_gpus; i++) {
		struct gpu *gpu = &gpus[i];

		gpu->dev_idx = i;
		gpu->stats.gpu = gpu;

		nret = nvmlDeviceGetHandleByIndex(i, &gpu->dev);
		if (nret != NVML_SUCCESS) {
			gpu_warn(gpu, "failed to get handle: %s\n",
							nvmlErrorString(nret));
			continue;
		}

		ret = get_gpu_stats(GPU_STAT_ALL & ~cfg.ignore_mask,
								&gpu->stats);
		if (ret) {
			gpu_warn(gpu, "failed to get stats\n");
			continue;
		}

		gpu->mask = gpu->stats.mask;
		gpu->flags |= GPU_F_ACTIVE;

		/*
		 * aggregated mask contains every stat supported by at least
		 * one installed GPU; defines set of printed table columns
		 * and set of nmon-sensors to be sent to MMCS
		 */
		aggr_mask |= gpu->mask;
	}


	/* aggr_mask sanity check, just in case */
	if (aggr_mask & cfg.ignore_mask || aggr_mask & ~GPU_STAT_ALL)
		warn("suspicious aggr_mask %llux\n",
					(unsigned long long) aggr_mask);
	if (!(aggr_mask & GPU_STAT_ALL)) {
		err("no stats to report, abort\n");
		ret = 8;
		goto free_out;
	}

	/* call the *_init part of the "output method" */
	if (in_framework)
		ret = update_sensors_init(gpus, nr_gpus, aggr_mask);
	else
		ret = print_table_init(gpus, nr_gpus, aggr_mask);

	if (ret) {
		ret = 9;
		goto free_out;
	}


	/* main stats reading cycle */

	while (!should_stop()) {
		/* update all per-GPU stats */
		for (i = 0; i < nr_gpus; i++) {
			struct gpu *gpu = &gpus[i];

			if (!(gpu->flags & GPU_F_ACTIVE))
				continue;

			gpu->stats.mask = 0;
			/* ignore ret for now */
			get_gpu_stats(aggr_mask & gpu->mask, &gpu->stats);
		}

		/* handle fresh stats */
		if (in_framework)
			update_sensors(gpus, nr_gpus, aggr_mask);
		else
			print_table(gpus, nr_gpus, aggr_mask);

		sleep(cfg.update_interval);
	}


//update_out:
	if (in_framework)
		update_sensors_exit();
free_out:
	free(gpus);

nvml_shut:
	nret = nvmlShutdown();
	if (nret != NVML_SUCCESS)
		warn("failed to shutdown NVML: %s\n", nvmlErrorString(nret));
syms_shut:
	shutdown_nvml_syms();

shm_dt:
	if (in_framework)
		shmdt(sbuf_desc);
	return ret;
}
