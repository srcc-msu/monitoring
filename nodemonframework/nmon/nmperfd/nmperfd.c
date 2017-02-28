#include <signal.h>
#include <unistd.h>
#include <errno.h>

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <nm_syslog.inc.c>
#include <nm_module.h>
#include <nm_modshm.h>

#include "nmpe.h"


#define _STRINGIFY(s)	#s
#define STRINGIFY(s)	_STRINGIFY(s)

#define PERF_ELM_LEN	sizeof(uint64_t)


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
#define ntohll(x)       htonll(x)


typedef enum {
	SENSOR_TYPE_SW = 0,
	SENSOR_TYPE_RAW,

	SENSOR_TYPE_NUM,
} sensor_type_t;


static int (*get_func_nmpe_close(sensor_type_t type))(int){
	int (*nmpe_close[SENSOR_TYPE_NUM])(int) = {
		[SENSOR_TYPE_SW] = nmpe_close_sw,
		[SENSOR_TYPE_RAW] = nmpe_close_raw,
	};
	return nmpe_close[type];
}


static int (*get_func_nmpe_open(sensor_type_t type))(int, uint64_t){
	int (*nmpe_open[SENSOR_TYPE_NUM])(int, uint64_t) = {
		[SENSOR_TYPE_SW] = nmpe_open_sw,
		[SENSOR_TYPE_RAW] = nmpe_open_raw,
	};
	return nmpe_open[type];
}


static int (*get_func_nmpe_read(sensor_type_t type))(int, uint64_t *){
	int (*nmpe_read[SENSOR_TYPE_NUM])(int, uint64_t *) = {
		[SENSOR_TYPE_SW] = nmpe_read_sw,
		[SENSOR_TYPE_RAW] = nmpe_read_raw,
	};
	return nmpe_read[type];
}


typedef struct {
	uint16_t	data_sid;
	uint16_t	conf_sid;
	char		*name;
} sensor_desc_t;

#define S(sens)						\
	[sens ## _IDX] =				\
		{					\
			.data_sid = sens,		\
			.conf_sid = sens + 1,		\
			.name = STRINGIFY(sens),	\
		}
static const sensor_desc_t sw_desc[TPCNT_SW_NUM] = {
	S(TPCNT_SW_CPU_CLOCK),
	S(TPCNT_SW_TASK_CLOCK),
	S(TPCNT_SW_PFLT),
	S(TPCNT_SW_CTX_SW),
	S(TPCNT_SW_CPU_MIG),
	S(TPCNT_SW_PFLT_MIN),
	S(TPCNT_SW_PFLT_MAJ),
};

static const sensor_desc_t raw_desc[CPU_PERF_NUM] = {
	S(CPU_PERF_FIXED01),
	S(CPU_PERF_FIXED02),
	S(CPU_PERF_FIXED03),
	S(CPU_PERF_COUNTER01),
	S(CPU_PERF_COUNTER02),
	S(CPU_PERF_COUNTER03),
	S(CPU_PERF_COUNTER04),
	S(CPU_PERF_COUNTER05),
	S(CPU_PERF_COUNTER06),
	S(CPU_PERF_COUNTER07),
	S(CPU_PERF_COUNTER08),
};
#undef S

const sensor_desc_t *get_sensors_desc(sensor_type_t type){
	const sensor_desc_t *d[SENSOR_TYPE_NUM] = {
		[SENSOR_TYPE_SW] = sw_desc,
		[SENSOR_TYPE_RAW] = raw_desc,
	};
	return d[type];
}


typedef struct {
	int		fails;
	unsigned	active		:1,
			configured	:1;
	uint64_t	conf;
	uint64_t	*values;
} sensor_state_t;

static sensor_state_t sw_state[TPCNT_SW_NUM];
static sensor_state_t raw_state[CPU_PERF_NUM];

sensor_state_t *get_sensors_state(sensor_type_t type){
	sensor_state_t *s[SENSOR_TYPE_NUM] = {
		[SENSOR_TYPE_SW] = sw_state,
		[SENSOR_TYPE_RAW] = raw_state,
	};
	return s[type];
}


int get_sensors_num(sensor_type_t type){
	const int n[SENSOR_TYPE_NUM] = {
		[SENSOR_TYPE_SW] = TPCNT_SW_NUM,
		[SENSOR_TYPE_RAW] = CPU_PERF_NUM,
	};
	return n[type];
}


typedef struct {
	size_t		dynlen;
	uint8_t		*dynval;
	size_t		sw_dynlen;
	uint8_t		*sw_dynval;
	size_t		raw_dynlen;
	uint8_t		*raw_dynval;
} perf_data_t;


static int num_elem = -1;
static int fails_max = 5;


static void close_sensors(sensor_type_t type){
	int i;
	int num_sens		= get_sensors_num(type);
	sensor_state_t *state	= get_sensors_state(type);
	int (*nmpe_close)(int)	= get_func_nmpe_close(type);

	for (i = 0; i < num_sens; i++){
		if (state[i].active){
			nmpe_close(i);
			state[i].active = 0;
		}

		if (state[i].values){
			free(state[i].values);
			state[i].values = NULL;
		}
	}
}


static int open_sensors(sensor_type_t type){
	int i;
	int num_sens			= get_sensors_num(type);
	sensor_state_t *state		= get_sensors_state(type);
	int (*nmpe_open)(int, uint64_t)	= get_func_nmpe_open(type);

	for (i = 0; i < num_sens; i++){
		state[i].values = NULL;
	}

	for (i = 0; i < num_sens; i++){
		if (!state[i].configured)
			continue;

		state[i].values = malloc(sizeof(uint64_t) * num_elem);
		if (!state[i].values){
			nm_syslog(LOG_ERR, "open_sensors: malloc: %s",
							strerror(errno));
			goto err_xit;
		}
	}

	for (i = 0; i < num_sens; i++){
		if (!state[i].configured)
			continue;

		if (nmpe_open(i, state[i].conf))
			goto err_xit;

		state[i].active = 1;
	}
	return 0;
err_xit:
	close_sensors(type);
	return -1;
}


static int calc_num_configured_sensors(sensor_type_t type){
	int i;
	int res = 0;
	int num_sens		= get_sensors_num(type);
	sensor_state_t *state	= get_sensors_state(type);

	for (i = 0; i < num_sens; i++){
		if (state[i].configured)
			res++;
	}
	return res;
}


static void init_headers(sensor_type_t type, uint8_t *dynval){
	int i;
	int csi = 0; /* configured sensor index */
	int num_sens			= get_sensors_num(type);
	const sensor_desc_t *desc	= get_sensors_desc(type);
	sensor_state_t *state		= get_sensors_state(type);

	for (i = 0; i < num_sens; i++){
		if (state[i].active){
			*(struct nm_tlv_hdr_t *)NM_GROUPADDR(dynval, PERF_ELM_LEN, num_elem, csi) =
			    NM_MONTYPE(desc[i].data_sid, PERF_ELM_LEN * num_elem);
			csi++;
		}
	}
}


static int init_perf(perf_data_t *data, struct nm_module_bufdesc_t *mdesc){
	int num_sw;
	int num_raw;

	memset(data, 0, sizeof(perf_data_t));

	if ((num_elem = nmpe_init()) < 0){
		nm_syslog(LOG_ERR, "%s", "initialisation nmpe failed");
		goto err_xit;
	}

	num_sw = calc_num_configured_sensors(SENSOR_TYPE_SW);
	num_raw = calc_num_configured_sensors(SENSOR_TYPE_RAW);

	if (num_sw == 0 && num_raw == 0){
		nm_syslog(LOG_ERR, "%s", "no sensors are configured");
		goto free_nmpe;
	}

	data->sw_dynlen = NM_DYNELEMLEN(PERF_ELM_LEN, num_elem) * num_sw;
	data->raw_dynlen = NM_DYNELEMLEN(PERF_ELM_LEN, num_elem) * num_raw;
	data->dynlen = data->sw_dynlen + data->raw_dynlen;

	data->dynval = nm_mod_buf_at(data->dynlen, mdesc);
	if (!data->dynval){
		nm_syslog(LOG_ERR, "%s", "cannot attach shm buffer");
		goto free_nmpe;
	}
	memset(data->dynval, 0, data->dynlen);

	if (num_sw == 0){
		data->sw_dynval = NULL;
	} else {
		data->sw_dynval = data->dynval;
		if (open_sensors(SENSOR_TYPE_SW)){
			nm_syslog(LOG_ERR, "%s", "cannot open sw sensors");
			goto dt_mod_buf;
		}
		init_headers(SENSOR_TYPE_SW, data->sw_dynval);
	}

	if (num_raw == 0){
		data->raw_dynval = NULL;
	} else {
		data->raw_dynval = data->dynval + data->sw_dynlen;
		if (open_sensors(SENSOR_TYPE_RAW)){
			nm_syslog(LOG_ERR, "%s", "cannot open raw sensors");
			goto close_sw_sensors;
		}
		init_headers(SENSOR_TYPE_RAW, data->raw_dynval);
	}
	return 0;

close_sw_sensors:
	close_sensors(SENSOR_TYPE_SW);
dt_mod_buf:
	nm_mod_buf_dt(data->dynval, mdesc);
	memset(data, 0, sizeof(perf_data_t));
free_nmpe:
	nmpe_free();
err_xit:
	return -1;
}


static void release_perf(perf_data_t *data, struct nm_module_bufdesc_t *mdesc){
	close_sensors(SENSOR_TYPE_SW);
	close_sensors(SENSOR_TYPE_RAW);
	if (data->dynval){
		nm_mod_buf_dt(data->dynval, mdesc);
		memset(data, 0, sizeof(perf_data_t));
	}
	nmpe_free();
}


static void copy_sensors_to_data(sensor_type_t type, uint8_t *dynval){
	int i;
	int j;
	int csi = 0; /* configured sensor index */
	uint64_t *values;
	int num_sens		= get_sensors_num(type);
	sensor_state_t *state	= get_sensors_state(type);

	for (i = 0; i < num_sens; i++){
		if (!state[i].configured)
			continue;

		for (j = 0; j < num_elem; j++){
			values = state[i].values;
#define PVADDR(I) NM_VECTADDR(dynval, PERF_ELM_LEN, num_elem, I, uint64_t)
			if (state[i].fails){
				PVADDR(csi)[j] = htonll((uint64_t)NM_CNT_ERR);
			} else {
				PVADDR(csi)[j] = htonll(values[j]);
			}
#undef PVADDR
		}
		csi++;
	}
}

#define NAME_LEN	20
#define CONF_LEN	10
#define VALUE_LEN	20
#define CPU_NUM_LEN	2
static void print_table_header(void){
	int j;

	printf("%*s%*s", NAME_LEN, "Sensor", CONF_LEN, "Config");
	for (j = 0; j < num_elem; j++){
		printf("%*s%*d", VALUE_LEN - CPU_NUM_LEN, "cpu",
							CPU_NUM_LEN, j);
	}
	putchar('\n');
}
#undef CPU_NUM_LEN


static void print_sensor_values(sensor_state_t *state){
	int j;
	uint64_t *values = state->values;

	for (j = 0; j < num_elem; j++){
		if (state->fails){
			printf("%*s", VALUE_LEN, "FAIL");
			continue;
		}
		printf("%*llu", VALUE_LEN, (long long unsigned)values[j]);
	}
}


#define SW_FLAGS_NUM	4
static void print_sw_config(uint64_t conf){
	/* print CONF_LEN - SW_FLAGS_NUM white spaces */
	printf("%*s", CONF_LEN - SW_FLAGS_NUM, "");

#define P(F, C)			\
	if (conf & F)		\
		putchar(C);	\
	else			\
		putchar('-');

	P(NMPE_SW_CONF_USERSPACE, 'u');
	P(NMPE_SW_CONF_KERNELSPACE, 'k');
	P(NMPE_SW_CONF_HYPERVISOR, 'h');
	P(NMPE_SW_CONF_IDLE, 'i');
#undef P
}
#undef SW_FLAGS_NUM


static void print_sensors_sw(void){
	int i;
	int num_sens			= get_sensors_num(SENSOR_TYPE_SW);
	const sensor_desc_t *desc	= get_sensors_desc(SENSOR_TYPE_SW);
	sensor_state_t *state		= get_sensors_state(SENSOR_TYPE_SW);

	print_table_header();

	for (i = 0; i < num_sens; i++){
		if (!state[i].configured)
			continue;

		printf("%*s", NAME_LEN, desc[i].name);
		print_sw_config(state[i].conf);
		print_sensor_values(&state[i]);
		putchar('\n');
	}
	putchar('\n');
}


static void print_sensors_raw(void){
	int i;
	int num_sens			= get_sensors_num(SENSOR_TYPE_RAW);
	const sensor_desc_t *desc	= get_sensors_desc(SENSOR_TYPE_RAW);
	sensor_state_t *state		= get_sensors_state(SENSOR_TYPE_RAW);

	print_table_header();

	for (i = 0; i < num_sens; i++){
		if (!state[i].configured)
			continue;

		printf("%*s", NAME_LEN, desc[i].name);
		printf("%#*llx", CONF_LEN, (long long unsigned)state[i].conf);
		print_sensor_values(&state[i]);
		putchar('\n');
	}
	putchar('\n');
}
#undef NAME_LEN
#undef CONF_LEN
#undef VALUE_LEN


void (*get_func_print_sensors(sensor_type_t type))(void){
	void (*p[SENSOR_TYPE_NUM])(void) = {
		[SENSOR_TYPE_SW] = print_sensors_sw,
		[SENSOR_TYPE_RAW] = print_sensors_raw,
	};
	return p[type];
}


static void read_sensors(int is_module, sensor_type_t type, uint8_t *dynval){
	int i;
	int num_sens				= get_sensors_num(type);
	const sensor_desc_t *desc		= get_sensors_desc(type);
	sensor_state_t *state			= get_sensors_state(type);
	int (*nmpe_read)(int, uint64_t *)	= get_func_nmpe_read(type);
	int (*nmpe_close)(int)			= get_func_nmpe_close(type);
	void (*print_sensors)(void)		= get_func_print_sensors(type);

	for (i = 0; i < num_sens; i++){
		if (!state[i].active)
			continue;

		if (!nmpe_read(i, state[i].values)){
			state[i].fails = 0;
			continue;
		}

		nm_syslog(LOG_ERR, "failed to read sensor '%s'", desc[i].name);
		if (state[i].fails < fails_max){
			state[i].fails++;
			continue;
		}

		nmpe_close(i);
		state[i].active = 0;
		nm_syslog(LOG_WARNING, "deactivated sensor '%s': "
					"error limit exceeded", desc[i].name);
	}

	if (is_module){
		copy_sensors_to_data(type, dynval);
	} else {
		print_sensors();
	}
}


static int perf_active = 1;

void perf_deactivate(int sig){
	perf_active = 0;
}


static void read_loop(int is_module, perf_data_t *data){
	while (perf_active){
		read_sensors(is_module, SENSOR_TYPE_SW, data->sw_dynval);
		read_sensors(is_module, SENSOR_TYPE_RAW, data->raw_dynval);

		sleep(NM_MON_PERIOD_SEC);
	}
}


static int parse_conf_sw(char *str, sensor_state_t *state){
	char *c = str;

	while (*c != '\0'){
		switch (*c){
		case 'u':
			state->conf |= NMPE_SW_CONF_USERSPACE;
			break;
		case 'k':
			state->conf |= NMPE_SW_CONF_KERNELSPACE;
			break;
		case 'h':
			state->conf |= NMPE_SW_CONF_HYPERVISOR;
			break;
		case 'i':
			state->conf |= NMPE_SW_CONF_IDLE;
			break;
		default:
			nm_syslog(LOG_ERR, "unknown flag '%c'", *c);
			return -1;
		}
		c++;
	}
	state->configured = 1;
	return 0;
}


static int parse_conf_raw(char *str, sensor_state_t *state){
	if (sscanf(str, "%llx", (long long unsigned *)&state->conf) != 1){
		nm_syslog(LOG_ERR, "invalid configuration '%s'", str);
		return -1;
	}
	state->configured = 1;
	return 0;
}


int (*get_func_parse_conf(sensor_type_t type))(char *, sensor_state_t *){
	int (*p[SENSOR_TYPE_NUM])(char *, sensor_state_t *) = {
		[SENSOR_TYPE_SW] = parse_conf_sw,
		[SENSOR_TYPE_RAW] = parse_conf_raw,
	};
	return p[type];
}


static int parse_sensor(char *str, sensor_type_t type){
	int i;
	char *name_str = str;
	char *conf_str;
	int num_sens				= get_sensors_num(type);
	const sensor_desc_t *desc		= get_sensors_desc(type);
	sensor_state_t *state			= get_sensors_state(type);
	int (*parse_conf)(char *, sensor_state_t *)
						= get_func_parse_conf(type);

	if (!(conf_str = strchr(str, ':'))){
		nm_syslog(LOG_ERR, "invalid sensor '%s'", str);
		return -1;
	}

	*conf_str = '\0';
	conf_str++;

	for (i = 0; i < num_sens; i++){
		if (!strcmp(name_str, desc[i].name))
			break;
	}
	if (i == num_sens){
		nm_syslog(LOG_ERR, "unknown sensor '%s'", str);
		return -1;
	}
	if (parse_conf(conf_str, &state[i])){
		nm_syslog(LOG_ERR, "invalid configuration '%s' for sensor '%s'",
							conf_str, desc[i].name);
		return -1;
	}
	return 0;
}


static void usage(FILE *s){
	int i;
	fprintf(s, APPNAME " [options]\n"
		"\t-h                   : show this message\n"
		"\t-s key               : module SHM key\n"
		"\t-c config            : config file\n"
		"\t-w sensors_list      : SW sensors list\n"
		"\t-r sensors_list      : RAW sensors list\n\n");
	fprintf(s, "SW sensors_list looks like \"sensor1:flag1[flag2[...]]"
				"[,sensor2:[flag1[flag2][...]]][,...]\"\n"
		"\tSupported sensors:\n");
	for (i = 0; i < TPCNT_SW_NUM; i++){
		fprintf(s, "\t\t%s\n", sw_desc[i].name);
	}
	fprintf(s, "\tSupported flags:\n"
		"\t\tu            : userspace\n"
		"\t\tk            : kernelspace\n"
		"\t\th            : hypervisor\n"
		"\t\ti            : idle\n");

	fprintf(s, "\nRAW sensors_list looks like "
		"\"sensor1:configuration[,sensor2:configuration][,...]\"\n"
		"\tSupported sensors:\n");
	for (i = 0; i < CPU_PERF_NUM; i++){
		fprintf(s, "\t\t%s\n", raw_desc[i].name);
	}
	fprintf(s, "\n");
}


static void parse_sensors_list(char *str, sensor_type_t type){
	char *c;

	c = strtok(str, ",");
	while (c){
		if (parse_sensor(c, type)){
			usage(stderr);
			exit(EXIT_FAILURE);
		}
		c = strtok(NULL, ",");
	}
}


#define CFG_FILE_OPT_NAME_MAX	30
#define CFG_FILE_OPT_VAL_MAX	30


static void get_conf(int argc, char *argv[], int *shmkey){
	int opt;
	int c;
	char ch;
	char cfg_opt[CFG_FILE_OPT_NAME_MAX + 1];
	char cfg_val[CFG_FILE_OPT_VAL_MAX + 1];
	char line[2 * (CFG_FILE_OPT_NAME_MAX + CFG_FILE_OPT_VAL_MAX)];
	char *config = NULL;
	char *sw_option_str = NULL;
	char *raw_option_str = NULL;
	FILE *f = NULL;

	while ((opt = getopt(argc, argv, "hs:c:w:r:")) != -1){
		switch (opt){
		case 'h': /* show help */
			usage(stdout);
			exit(EXIT_SUCCESS);
		case 's': /* module SHM key */
			*shmkey = atoi(optarg);
			break;
		case 'c': /* config file */
			config = strdup(optarg);
			break;
		case 'w': /* SW counters configuration */
			sw_option_str = strdup(optarg);
			break;
		case 'r': /* RAW conuters configuration */
			raw_option_str = strdup(optarg);
			break;
		default:
			usage(stderr);
			exit(EXIT_FAILURE);
		}
	}

	if (sw_option_str){
		parse_sensors_list(sw_option_str, SENSOR_TYPE_SW);
	}

	if (raw_option_str){
		parse_sensors_list(raw_option_str, SENSOR_TYPE_RAW);
	}

	if (!config)
		config = CONFIGFILE;

	if (!(f = fopen(config, "r"))){
		nm_syslog(LOG_WARNING, "cannot open config file '%s':\n",
								config);
		goto xit;
	}

	while (fgets(line, sizeof(line), f)){

		/* If the first non-white-space char is '#', it's a comment.
		   Line does not contain non-white-space char is ignored. */
		c = sscanf(line, " %1c", &ch);
		if ((c == 1 && ch == '#') || c == EOF)
			continue;

		c = sscanf(line, "%" STRINGIFY(CFG_FILE_OPT_NAME_MAX) "s "
				 "%" STRINGIFY(CFG_FILE_OPT_VAL_MAX) "s",
				 cfg_opt, cfg_val);
		if (c != 2){
			nm_syslog(LOG_ERR, "error parsing cfg line '%s'", line);
			exit(EXIT_FAILURE);
		}

		if (!strcmp(cfg_opt, "sw")){
			/* skip if defined cmdline option */
			if (sw_option_str)
				continue;

			if (parse_sensor(cfg_val, SENSOR_TYPE_SW)){
				exit(EXIT_FAILURE);
			}
		} else if (!strcmp(cfg_opt, "raw")){
			/* skip if defined cmdline option */
			if (raw_option_str)
				continue;

			if (parse_sensor(cfg_val, SENSOR_TYPE_RAW)){
				exit(EXIT_FAILURE);
			}
		} else {
			nm_syslog(LOG_ERR, "unknown cfg line option '%s'", cfg_opt);
			exit(EXIT_FAILURE);
		}
	}
	fclose(f);
xit:
/*
	// It was copied from another module. I doubt the correctness of this.
	if (config != (char *) CONFIGFILE)
		free(config);
*/
	if (sw_option_str)
		free(sw_option_str);
	if (raw_option_str)
		free(raw_option_str);
}


int main(int argc, char *argv[]){
	int shmkey = -1;
	struct sigaction sigact;
	perf_data_t data;
	struct nm_module_bufdesc_t *bufdesc = NULL;

	memset(sw_state, 0, sizeof(sw_state));
	memset(raw_state, 0, sizeof(raw_state));
	get_conf(argc, argv, &shmkey);

	if (shmkey != -1){
		bufdesc = nm_mod_bufdesc_at(shmkey);
		if (!bufdesc){
			nm_syslog(LOG_ERR, "%s", "cannot commuticate with "
							"master process");
			exit(EXIT_FAILURE);
		}
	}

	if (init_perf(&data, bufdesc)){
		nm_syslog(LOG_ERR, "%s", "initialization failed");
		exit(EXIT_FAILURE);
	}

        nm_syslog(LOG_NOTICE, "%s", "starting");
	if (shmkey != -1){
		signal(SIGHUP, SIG_IGN);
		signal(SIGINT, SIG_IGN);
		memset(&sigact, 0, sizeof(struct sigaction));
		sigact.sa_handler = perf_deactivate;
		sigemptyset(&sigact.sa_mask);
		sigaction(SIGTERM, &sigact, NULL);
		read_loop(1, &data);
        } else {
		read_loop(0, &data);
	}

	nm_syslog(LOG_NOTICE, "%s", "stopping");
	release_perf(&data, bufdesc);
	nm_mod_bufdesc_dt(bufdesc);
	return 0;
}
