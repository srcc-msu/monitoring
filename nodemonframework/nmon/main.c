#define _GNU_SOURCE
#define _BSD_SOURCE

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <netinet/in.h>
#include <linux/if.h>
#include <signal.h>
#include <netdb.h>
#include <time.h>
#include <unistd.h>
// sys/file.h for flock()
#include <sys/file.h>
#include <fcntl.h>

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <nm_dumb.h>
#ifdef _USERLAND
#include <nm_dietquirk.h>
#endif

#include <nm_module.h>
#include <nm_syslog.inc.c>
#include <nm_control.h>

#include <try_oom_adj.h>

#define OOM_ENV_VARNAME "KNMONCTLD_OOM_ADJ"

/* Old kernel monitoring */
//#define NM_KCTL_FILE	"/proc/knagent/ctl"
#define NM_KCTL_FILE	"/proc/clustrx/knmon/ctl"


#define NM_ROUTE_FILE	"/proc/net/route"

#ifndef PIDFILE
# ifdef _USERLAND
#  define PIDFILE	"/var/run/nmond.pid"
# else
#  define PIDFILE	"/var/run/knmonctld.pid"
# endif
#endif

#define NO_PIDFILE	"none"

#define NM_MODULE_SHOULD_RUN		1
#define NM_MODULE_SHOULD_STOP		2
#define NM_MODULE_SHOULD_RESTART	4
#define NM_MODULE_DEFAULT		8
#define NM_MODULE_STATISTICS		16
#define NM_MODULE_STANDALONE		32
#define NM_MODULE_MANDATORY		64
#define NM_MODULE_HOPSA			128


struct nm_module_desc_t {
	const char	*name;
	uint32_t	flags;
	pid_t		pid;
	size_t          bufsize;
	unsigned short	retries;
	struct nm_module_bufdesc_t *shm_bufdesc;
	void		*buf;
	struct timeval	load_ts;
};

#define NM_HWCK_RETR_MAX	60
#define NM_HWCK_RETR_INTERVAL	5

// Exit codes.
// FIXME: use <sysexits.h>?
#define EXIT_GENERAL_FAILURE		1
#define EXIT_OSERR			2
#define EXIT_CONFIG			3
#define EXIT_MEMORY			4
#define EXIT_NOPERM			5

#ifdef _USERLAND
static const char *nm_data_sig = "MMCS.NAG";
#else
static const char *nm_knmond = "knmond";
static const char *nm_knmhwsd = "knmhwsd";
#endif




static int nm_alive = 1;

#define RETR	5
static struct nm_module_desc_t nm_modules[] = {
/*       name, flags, pid, bufsize, retries, shm_bufdesc, buf, load_ts */
	{"hybmond",	NM_MODULE_STANDALONE,	0, 0, RETR, NULL, NULL, {0, 0}},
#ifdef _USERLAND
	{"nmfsd",	NM_MODULE_STATISTICS,	0, 0, RETR, NULL, NULL, {0, 0}},
	{"nmibd",	NM_MODULE_DEFAULT,	0, 0, RETR, NULL, NULL, {0, 0}},
	{"nmiod",	NM_MODULE_DEFAULT,	0, 0, RETR, NULL, NULL, {0, 0}},
	{"nmipmid",	NM_MODULE_DEFAULT,	0, 0, RETR, NULL, NULL, {0, 0}},
#ifdef SENSMOD
	{"nmsensd",	NM_MODULE_DEFAULT,	0, 0, RETR, NULL, NULL, {0, 0}},
#endif
	{"nmsmartd",	NM_MODULE_STATISTICS,	0, 0, RETR, NULL, NULL, {0, 0}},
	{"nmhopsad",	NM_MODULE_HOPSA,	0, 0, RETR, NULL, NULL, {0, 0}},
	{"nmgpunvd",	NM_MODULE_DEFAULT,	0, 0, RETR, NULL, NULL, {0, 0}},
	{"nmperfd",	NM_MODULE_DEFAULT,	0, 0, RETR, NULL, NULL, {0, 0}},
#endif
	{NULL,		0,			0, 0, 0, NULL, NULL, {0, 0}}
};
#undef RETR


static void inline prep_hints(struct addrinfo *h){
	bzero(h, sizeof(struct addrinfo));
	h->ai_family = AF_INET;
}


static int nm_getaddr(char *str, void *addrbuf){
	struct addrinfo hints, *addr = NULL;
	
	prep_hints(&hints);
	if (getaddrinfo(str, NULL, &hints, &addr)){
		free(addr);
		return -1;
	}
	memcpy(addrbuf, &(((struct sockaddr_in *)(addr->ai_addr))->sin_addr), sizeof(struct in_addr));
	free(addr);
	return 0;
}


static int prestart_module(struct nm_module_desc_t *moddesc, const char *mod){
	int i = 0;

	while (moddesc[i].name){
		if (!strcmp(moddesc[i].name, mod)){
			moddesc[i].flags |= NM_MODULE_SHOULD_RUN;
			
			return 0;
		}
		i++;
	}

	return -1;
}


/* Non-reentable code */
static int preinit_modules(struct nm_module_desc_t *moddesc, char *mods){
	char *mod;
	
	if (!mods)
		return 0;
		
	mod = strtok(mods, ",");
	if (prestart_module(moddesc, mod))
		return -1;
	
	while ((mod = strtok(NULL, ","))){
		if (prestart_module(moddesc, mod))
			return -1;
	}
	
	return 0;
}


struct kmod_opts {
	char *kmod_name;
	char *kmod_opt;
};


char *lookup_kmod_opt(char *name, struct kmod_opts *kmo){
	int i = 0;
	
	if (!kmo)
		return NULL;
	
	while (kmo[i].kmod_name){
		if (!strcmp(kmo[i].kmod_name + strlen("opt_"), name))
			return kmo[i].kmod_opt;
		i++;
	}
	
	return NULL;
}


#define MODPROBE_STR	"/sbin/modprobe"
#ifdef _USERLAND
static void load_kmod(char *name){
#else
static void load_kmod(char *name, char *params){
	int rv;
#endif
	char *cmd;
	
	nm_syslog(LOG_INFO, "%s %s", "trying to load kernel module", name);
#ifdef _USERLAND
	if (asprintf(&cmd, MODPROBE_STR " %s", name) != -1){
#else
	if (params)
		rv = asprintf(&cmd, MODPROBE_STR " %s %s", name, params);
	else
		rv = asprintf(&cmd, MODPROBE_STR " %s", name);
	
	if (rv != -1){
#endif
		system(cmd);
		free(cmd);
	}
}


#ifdef _USERLAND
static void load_kmods(char *kmod_list){
#else
static void load_kmods(char *kmod_list, struct kmod_opts *kmo, struct nm_hw_status_t *hws){
#endif
	char *tmp;
	char *mod;
#ifndef _USERLAND
	char *opt;

	if (hws){
		if (hws->hw_msg)
			asprintf(&opt, "hwstatus=%d hwmessage='%s'", hws->code, hws->hw_msg);
		else
			asprintf(&opt, "hwstatus=%d", hws->code);

		load_kmod((char *) nm_knmhwsd, opt);
		free(opt);
	}
#endif
	if (!kmod_list)
		return;
	
	tmp = strdup(kmod_list);
	mod = strtok(tmp, ",");
#ifdef _USERLAND
	load_kmod(mod);
#else
	opt = lookup_kmod_opt(mod, kmo);
	load_kmod(mod, opt);
#endif
	
	while ((mod = strtok(NULL, ","))){
#ifdef _USERLAND
		load_kmod(mod);
#else
		opt = lookup_kmod_opt(mod, kmo);
		load_kmod(mod, opt);
#endif
	}
	free(tmp);
}


#ifndef _USERLAND
static void unload_kmod(char *mod){
	int rv;
	char *cmd;
	
	nm_syslog(LOG_INFO, "%s %s", "unloading kernel module", mod);
	rv = asprintf(&cmd, "modprobe -r %s", mod);
	if (rv != -1){
		system(cmd);
		free(cmd);
	}
}


static void unload_kmods(char *kmod_list){
	char *tmp;
	char *mod;
	
	unload_kmod((char *) nm_knmhwsd);
	if (kmod_list){
		tmp = strdup(kmod_list);
		
		mod = strtok(tmp, ",");
		unload_kmod(mod);
		
		while ((mod = strtok(NULL, ",")))
			unload_kmod(mod);
		
		free(tmp);
	}
	unload_kmod((char *) nm_knmond);
}
#endif


// Auxiliary to main().
// Covers only currently known cases.
static int
errno_to_exitcode(int se)
{
	if (se == ENOMEM)
		return EXIT_MEMORY;
	if (se == ENOENT)
		return EXIT_CONFIG;
	return EXIT_OSERR;
}


static void start_module(struct nm_module_desc_t *desc){
	int unused;
	int shmid = -1;
	int se;
	pid_t pid;
	struct timeval tv;
	const char err_msg_str[] = "cannot start module";
	char *shmstr;
	
	if (!(desc->flags & NM_MODULE_STANDALONE)){
		shmid = shmget(IPC_PRIVATE, sizeof(struct nm_module_bufdesc_t), 
		    IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
		if (shmid < 0){
			nm_syslog(LOG_ERR, "%s %s: shmget: %s", err_msg_str,
						desc->name, strerror(errno));
			return;
		}
		desc->shm_bufdesc = shmat(shmid, NULL, 0);
		shmctl(shmid, IPC_RMID, NULL);
		if (desc->shm_bufdesc == (void *)-1){
			nm_syslog(LOG_ERR, "%s %s: shmctl: %s", err_msg_str,
						desc->name, strerror(errno));
			desc->shm_bufdesc = NULL;
			return;
		}
		bzero(desc->shm_bufdesc, sizeof(struct nm_module_bufdesc_t));
	}
	
	pid = fork();
	switch (pid){
	case -1:
		nm_syslog(LOG_ERR, "%s %s: fork: %s", err_msg_str,
						desc->name, strerror(errno));
		desc->flags &= ~NM_MODULE_SHOULD_RUN;
		shmdt(desc->shm_bufdesc);
		desc->shm_bufdesc = NULL;
		desc->pid = 0;
		break;
	case 0:
		if (prctl(PR_SET_PDEATHSIG, SIGTERM)){
			se = errno;
			nm_syslog(LOG_ERR, "%s %s: prctl: %s", err_msg_str,
						desc->name, strerror(se));
			_exit(errno_to_exitcode(se));
		}

		if (desc->flags & NM_MODULE_STANDALONE){
			execlp(desc->name, desc->name, NULL);
		} else {
			unused = asprintf(&shmstr, "%d", shmid);
			execlp(desc->name, desc->name, "-s", shmstr, NULL);
		}

		se = errno;
		nm_syslog(LOG_ERR, "%s %s: execlp: %s", err_msg_str,
						desc->name, strerror(se));
		_exit(errno_to_exitcode(se));
	default:
		gettimeofday(&tv, NULL);
		memcpy(&desc->load_ts, &tv, sizeof(struct timeval));
		desc->pid = pid;
		nm_syslog(LOG_INFO, "module %s started", desc->name);
	}
}


static int start_modules(struct nm_module_desc_t *moddesc){
	int i = 0;
	int err = 0;
	
	while (moddesc[i].name){
		if (!(moddesc[i].pid) && (moddesc[i].flags & NM_MODULE_SHOULD_RUN)){
			start_module(&(moddesc[i]));
			err = -1;
		}
		i++;
	}
	
	return err;
}


void nm_handlechild(int signum, siginfo_t *si, void *unused){
	int status;
	int se; /* save errno*/
	pid_t pid;
	struct nm_module_desc_t *mod;
	
	se = errno;
	while ((pid = waitpid(-1, &status, WNOHANG)) > 0){
		
		for (mod = nm_modules; mod->name; mod++){
			if (mod->pid != pid)
				continue;

//TODO: maybe check load_ts needed
			if ((WIFEXITED(status) && WEXITSTATUS(status) != 0)
			    || !mod->retries){
				mod->flags |= NM_MODULE_SHOULD_STOP;
			} else {
				mod->flags |= NM_MODULE_SHOULD_RESTART;
				mod->retries--;
			}
			break;
		}
	}
	errno = se;
}


void nm_term(int sig){
	killpg(getpid(), sig);
	nm_alive = 0;
	signal(sig, SIG_IGN);
}


static void nm_usage(FILE *f, struct nm_module_desc_t *moddesc){
	int i = 0;

	fprintf(f, APPNAME " [options] [--] [HW check options]\n"
	    "\t-local_host addr	: address bind to\n"
	    "\t-local_port port	: port bind to\n"
	    "\t-host addr		: remote host\n"
	    "\t-port port		: remote port\n"
	    "\t-cac_mode mode		: control address checking mode\n"
	    "\t-pretend_host addr	: pretend address\n"
	    "\t-addr_if interface	: use interface address\n"
	    "\t-conf configfile	: config file name\n"
	    "\t-mods modlist		: comma separated list of modules\n"
	    "\t-randomize_start	: random delay before start\n"
	    "\t-h			: show this message\n"
#ifdef _USERLAND
	    "\t-ifs ifslist		: comma separated list of interfaces (ethX,ibX)\n"
	    "\t-no_send_cpu_mem	: don't send dumb sensors (cpu,mem)\n"
#endif
	    "\t-nohwck		: disable HW check\n"
	    "\nAvailable modules:");
	
	while (moddesc[i].name){
		fprintf(f, "%c%s", (i ? ',' : ' '), moddesc[i].name);
		i++;
	}
	fprintf(f, "\n\n");
}


static void nm_srand(nm_server_state *sst)
{
	int urd, unused;
	int randint = 0;

	if ((urd = open("/dev/urandom", O_RDONLY)) != -1) {
		unused = read(urd, &randint, sizeof(randint));
		close(urd);
	}
	srand(getpid() ^ time(NULL) ^
	    randint ^ (sst->cmd_hdr->client_host.b4[0]));
}


#ifdef _USERLAND
static int tv_cmp(struct timeval *tv0, struct timeval *tv1){
	if (tv0->tv_sec < tv1->tv_sec)
		return -1;
	if (tv0->tv_sec > tv1->tv_sec)
		return 1;
	if (tv0->tv_usec < tv1->tv_usec)
		return -1;
	if (tv0->tv_usec > tv1->tv_usec)
		return 1;
	return 0;
}


static void tv_fix(struct timeval *tv){
	if ((int) tv->tv_usec < 0){
		tv->tv_sec--;
		tv->tv_usec += 1000000;
	}
	
	if ((int) tv->tv_usec >= 1000000){
		tv->tv_sec++;
		tv->tv_usec -= 1000000;
	}
}


static void tv_sub(struct timeval *tv0, struct timeval *tv1){
	tv0->tv_sec -= tv1->tv_sec;
	tv0->tv_usec-= tv1->tv_usec;
	tv_fix(tv0);
}


inline static void update_data_hdr(uint32_t seq_num,
				struct nm_data_hdr_t *hdr,
				struct timeval *tv){
	hdr->seq_num = htonl(seq_num);
	hdr->ts_m    = htonl(tv->tv_sec  / 1000000);
	hdr->ts_sec  = htonl(tv->tv_sec  % 1000000);
	hdr->ts_usec = htonl(tv->tv_usec % 1000000);
}


void update_strm(struct nm_data_hdr_t *hdr, nm_kcmd *cmd_hdr)
{
	// For flags, copy only 2 LSB bits (address type.)
	// FIXME: as soon as this works only with IPv4 addresses now,
	// flags shall be 0 always. This would change later.
	hdr->flags = 3 & cmd_hdr->flags;
	memcpy(hdr->cookie, cmd_hdr->cookie, NM_COOKIE_SIZE);
	memcpy(&(hdr->client_host), &(cmd_hdr->client_host), 4);
}


static size_t calc_bufsize_modules(const uint32_t flags_mask){
	size_t res;
	struct nm_module_desc_t *mod;

	res = 0;
	for (mod = nm_modules; mod->name; mod++){
		if (mod->buf && (mod->flags & flags_mask)){
			res += mod->bufsize;
		}
	}
	return res;
}


static void nm_clean_module(struct nm_module_desc_t *mod){
	if (mod->shm_bufdesc){
		shmdt(mod->shm_bufdesc);
		mod->shm_bufdesc = NULL;
	}

	if (mod->buf){
		shmdt(mod->buf);
		mod->buf = NULL;
	}

	mod->bufsize = 0;
}


static void nm_clean_modules(){
	struct nm_module_desc_t *mod;

	for (mod = nm_modules; mod->name; mod++){
		nm_clean_module(mod);
	}
}


inline static void nm_try_restart_module(struct nm_module_desc_t *mod){
	kill(mod->pid, SIGTERM);
	nm_clean_module(mod);
	if (!mod->retries){
		mod->flags &= ~NM_MODULE_SHOULD_RUN;
		nm_syslog(LOG_INFO, "stop module %s", mod->name);
	} else {
		mod->retries--;
		start_module(mod);
		nm_syslog(LOG_INFO, "trying to restart module %s", mod->name);
	}
}


inline static int check_modules(const uint32_t flags_mask){
	int res;
	int shmid;
	struct nm_module_desc_t *mod;
	struct nm_module_bufdesc_t *bufdesc;

	res = 0;
	for (mod = nm_modules; mod->name; mod++){
		if (!(mod->flags & flags_mask)
		    || !(mod->flags & NM_MODULE_SHOULD_RUN))
			continue;

		if (mod->flags & NM_MODULE_SHOULD_STOP){
			nm_clean_module(mod);
			mod->flags &= ~(NM_MODULE_SHOULD_STOP|
						NM_MODULE_SHOULD_RUN);
			nm_syslog(LOG_INFO, "module stopped %s", mod->name);
			res = -1;
			continue;
		}

		if (mod->flags & NM_MODULE_SHOULD_RESTART){
			nm_syslog(LOG_INFO, "trying to restart %s", mod->name);
			nm_clean_module(mod);
			mod->flags &= ~NM_MODULE_SHOULD_RESTART;
			start_module(mod);
			res = -1;
			continue;
		}

		bufdesc = mod->shm_bufdesc;

		if (bufdesc->buf_flags & NM_BUF_SHMID_CHANGED){
			mod->bufsize = bufdesc->buf_size;
			shmid = bufdesc->buf_shmid;
			bufdesc->buf_flags &= ~NM_BUF_SHMID_CHANGED;

			res = -1;

			if (mod->buf)
				shmdt(mod->buf);

			if (!mod->bufsize){
				mod->buf = NULL;
				continue;
			}

			mod->buf = shmat(shmid, NULL, 0);
			if (mod->buf != (void *)-1)
				continue;

			nm_syslog(LOG_ERR, "cannot attach buffer of module %s:"
				" shmat: %s", mod->name, strerror(errno));

			nm_try_restart_module(mod);
		}
	}
	return res;
}


inline static void *collect_buffers_internal(uint8_t *buf,
		nm_server_state *sst)
{
	uint8_t *tmp = buf;
	// Provide shortcuts to reduce code.
	struct nm_cpu_info_t *cpu_info = sst->cpu_info;
	struct nm_mem_info_t *mem_info = sst->mem_info;
	struct nm_iface_info_t *ifs_info = sst->iface_info;
	struct nm_hw_status_t *hw_stat = sst->hwstat;

	if (cpu_info){
		nm_getinfo_cpus(cpu_info);
		memcpy(tmp, cpu_info->dynval, cpu_info->dynlen);
		tmp += cpu_info->dynlen;
	}

	if (mem_info){
		nm_getinfo_mem(mem_info);
		if (mem_info->dynmlen){
			memcpy(tmp, mem_info->dynvalmem, mem_info->dynmlen);
			tmp += mem_info->dynmlen;
		}
		if (mem_info->dynhlen){
			memcpy(tmp, mem_info->dynvalhtlb, mem_info->dynhlen);
			tmp += mem_info->dynhlen;
		}
		if (mem_info->dynvlen){
			memcpy(tmp, mem_info->dynvalvm, mem_info->dynvlen);
			tmp += mem_info->dynvlen;
		}
	}

	if (ifs_info){
		nm_getinfo_ifaces(ifs_info);
		memcpy(tmp, ifs_info->dynval, ifs_info->dynlen);
		tmp += ifs_info->dynlen;
	}

	if (hw_stat){
		memcpy(tmp, hw_stat->dynval, hw_stat->dynlen);
		tmp += hw_stat->dynlen;
	}

	return tmp;
}


inline static uint8_t *collect_buffers_modules(const uint32_t flags_mask, uint8_t *buf){
	uint8_t *tmp = buf;
	struct nm_module_desc_t *mod = nm_modules;

	for (; mod->name; mod++){
		if (mod->buf && (mod->flags & flags_mask)){
			memcpy(tmp, mod->buf, mod->bufsize);
			tmp += mod->bufsize;
		}
	}

	return tmp;
}


static int init_strm(size_t internal_data_len,
			struct nm_kcmd *cmd_hdr,
			struct nm_strm_buf_t *strm)
{
	uint8_t *tmp;
	struct nm_data_hdr_t *data_hdr;

	strm->data_len = internal_data_len;
	strm->data_len += calc_bufsize_modules(strm->flags_mask);

	if (strm->data_len){
		strm->data_len += sizeof(struct nm_data_hdr_t) + NM_TLV_END_SIZE + NM_HMAC_SIZE;
		if (!(tmp = realloc(strm->data, strm->data_len))){
			if (strm->data)
				free(strm->data);

			return -1;
		}
		strm->data = tmp;
		bzero(strm->data, strm->data_len);

		strm->seq_num = 0;
		data_hdr = (struct nm_data_hdr_t *)strm->data;
		strncpy((char *)data_hdr->signature, nm_data_sig, NM_SIGNATURE_SIZE);
		data_hdr->version = htons(1);
		data_hdr->msg_type = 1;
		data_hdr->strm_num = strm->num;
		data_hdr->ssrc = rand();

		update_strm(data_hdr, cmd_hdr);
	} else {
		if (strm->data)
			free(strm->data);
			strm->data = NULL;
	}

	return 0;
}


#define IS_NOT_EMPTY_STRM(S)	S->flags_mask
static void cleanup_strms(struct nm_strm_buf_t *strms){
	struct nm_strm_buf_t *strm;

        for (strm = strms; IS_NOT_EMPTY_STRM(strm); strm++){
		if (strm->data)
			free(strm->data);
		bzero(strm, sizeof(struct nm_strm_buf_t));
	}
}
#endif /* _USERLAND */

static struct sockaddr_in ra;

#ifdef _USERLAND
// List of streams supported by the agent.
// We declare it statically and then export pointer to it.
static struct nm_strm_buf_t static_send_strms[] = {
//	num, 			period, 	flags_mask, seq_num, next_send, data_len, *data
	{NM_STRM_NUM_MON, NM_MON_PERIOD_SEC, NM_MODULE_DEFAULT,
	 0, -1, {0, 0}, 0, NULL},
	{NM_STRM_NUM_STAT, NM_STAT_SEND_PERIOD_SEC, NM_MODULE_STATISTICS,
	 0, -1, {0, 0}, 0, NULL},
	{NM_STRM_NUM_HOPSA, NM_HOPSA_PERIOD_SEC, NM_MODULE_HOPSA,
	 0, -1, {0, 0}, 0, NULL},
	// Stream array terminator
	{0, 0, 0, 0, 0, {0, 0}, 0, NULL}
};
struct nm_strm_buf_t *send_strms = static_send_strms;

static void
proceed_with_stream(
		nm_server_state *sst,
		struct nm_strm_buf_t *strm,
		size_t const_mon_len)
{
	struct timeval tv_curr;
	size_t tmp_len;
	uint8_t *tmp;
	struct nm_kcmd *cmd_hdr = sst->cmd_hdr;

	gettimeofday(&tv_curr, NULL);
	if (tv_cmp(&strm->next_send, &tv_curr) <= 0){

		// Check sending window. If it prohibits sending now,
		// give 1 second step before the next check.
		// This allows reasonable leizure time when the stream
		// is idle.
		// TODO: when sending interval will be configured, change
		// this to sending interval.
		if (strm->window == 0) {
			strm->next_send.tv_sec++;
			return;
		}
		if (strm->window > 0)
			--strm->window;

		if (check_modules(strm->flags_mask)){
			if (strm->flags_mask & NM_MODULE_DEFAULT)
				tmp_len = const_mon_len;
			else
				tmp_len = 0;

			init_strm(tmp_len, cmd_hdr, strm);
		}

		if (strm->data){
			tmp = collect_buffers_modules(strm->flags_mask, strm->data + sizeof(struct nm_data_hdr_t));

			if (strm->flags_mask & NM_MODULE_DEFAULT){
				collect_buffers_internal(tmp, sst);
			}

			update_data_hdr(strm->seq_num++,(struct nm_data_hdr_t *)strm->data, &tv_curr);

			sendto(sst->sock, strm->data, strm->data_len, 0, (struct sockaddr *) &ra, sizeof(struct sockaddr_in));
		}
		memcpy(&strm->next_send, &tv_curr, sizeof(struct timeval));
		strm->next_send.tv_sec += strm->period;
	}
}

#endif

static int
nm_server(nm_server_state *sst)

{
	int err = 0;
	fd_set fds;
#ifdef _USERLAND
	struct nm_strm_buf_t *strm;
#endif
	struct nm_kcmd *cmd_hdr = sst->cmd_hdr;

#ifndef _USERLAND
	int nm_ctl = open(NM_KCTL_FILE, O_WRONLY|O_NONBLOCK);
	if (nm_ctl < 0){
		nm_term(SIGTERM);
		nm_syslog(LOG_ERR, "%s", "failed to open control interface");
		return -1;
	}
	sst->nm_ctl = nm_ctl;
	// Initial configuring of target address, if present.
	if (cmd_hdr->tgt_host.b4[0] && cmd_hdr->tgt_port)
		ioctl(nm_ctl, KNM_IOCTL, cmd_hdr);
#else
	size_t const_mon_len = 0;
	size_t tmp_len;
	struct timeval tv_curr, tv_delay;

	if (sst->iface_info){
		nm_getinfo_ifaces(sst->iface_info);
		const_mon_len += sst->iface_info->dynlen;
	}

	if (sst->cpu_info){
		nm_getinfo_cpus(sst->cpu_info);
		const_mon_len += sst->cpu_info->dynlen;
	}
	
	if (sst->mem_info){
		nm_getinfo_mem(sst->mem_info);
		const_mon_len += sst->mem_info->dynmlen;
		const_mon_len += sst->mem_info->dynhlen;
		const_mon_len += sst->mem_info->dynvlen;
	}
	
	if (sst->hwstat){
		const_mon_len += sst->hwstat->dynlen;
	}
	
	usleep((rand() % 2000000) + 1000000);

	for (strm = send_strms; IS_NOT_EMPTY_STRM(strm); strm++){
		if (strm->flags_mask & NM_MODULE_DEFAULT)
			tmp_len = const_mon_len;
		else
			tmp_len = 0;

		if (init_strm(tmp_len, cmd_hdr, strm)){
			err = -1;
			goto err_xit;
		}
	}

	gettimeofday(&tv_curr, NULL);
	for (strm = send_strms; IS_NOT_EMPTY_STRM(strm); strm++){
		memcpy(&strm->next_send, &tv_curr, sizeof(struct timeval));
	}
#endif
	ra.sin_family = AF_INET;
	ra.sin_port = 0;
	memcpy(&(ra.sin_addr.s_addr), &(cmd_hdr->tgt_host), sizeof(uint32_t));
	
	while (nm_alive){
#ifdef _USERLAND
		for (strm = send_strms; IS_NOT_EMPTY_STRM(strm); strm++){
			proceed_with_stream(sst, strm, const_mon_len);
		}
		
		gettimeofday(&tv_curr, NULL);
		strm = send_strms;
		memcpy(&tv_delay, &strm->next_send, sizeof(struct timeval));
		strm++;
		for (; IS_NOT_EMPTY_STRM(strm); strm++){
			if(tv_cmp(&strm->next_send, &tv_delay) < 0)
				memcpy(&tv_delay, &strm->next_send, sizeof(struct timeval));
		}
		tv_sub(&tv_delay, &tv_curr);
#endif
		FD_ZERO(&fds);
		FD_SET(sst->sock, &fds);
#ifdef _USERLAND
		if (select(sst->sock+1, &fds, NULL, NULL, &tv_delay) > 0)
#else
		if (select(sst->sock+1, &fds, NULL, NULL, NULL) > 0)
#endif
		{
			if (FD_ISSET(sst->sock, &fds)){
				read_and_process_control_packet(sst);

				/* copying tgt_host and tgt_addr to ra */
				ra.sin_port = cmd_hdr->tgt_port;
				memcpy(&(ra.sin_addr.s_addr), &(cmd_hdr->tgt_host), sizeof(uint32_t));
#ifdef _USERLAND
				for (strm = send_strms; IS_NOT_EMPTY_STRM(strm); strm++){
					/* skip streams without data */
					if (!strm->data){
						continue;
					}
					update_strm((struct nm_data_hdr_t *)strm->data, cmd_hdr);
				}
#endif
			} /* if (sock is readable) */
		} /* select */
	
	} /* while (nm_alive) */
#ifdef _USERLAND
err_xit:
	cleanup_strms(send_strms);
#endif
	return err;
}
#undef IS_NOT_EMPTY_STRM


static uint32_t nm_get_addr_if(char *ifname){
	int s;
	uint32_t addr = INADDR_ANY;
	struct ifconf if_list;
	
	if_list.ifc_req = malloc(256 * sizeof(struct ifreq));
	if_list.ifc_len = 256 * sizeof(struct ifreq);

//TODO: error handling
	s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	ioctl(s, SIOCGIFCONF, &if_list);
	close(s);
	
	for (s = 0; s < (int)(if_list.ifc_len/sizeof(struct ifreq)); s++){
		if (!strcmp(ifname, if_list.ifc_req[s].ifr_ifrn.ifrn_name)){
			addr = ((struct sockaddr_in *) &(if_list.ifc_req[s].ifr_ifru.ifru_addr))->sin_addr.s_addr;
			break;
		}
	}
	
	free(if_list.ifc_req);
	
	return addr;
}


static char *nm_get_default_route_if(void){
	uint32_t route;
	FILE *f;
	char *tmp;
	char *ifname = NULL;
	
	if (!(f = fopen(NM_ROUTE_FILE, "r")))
		return NULL;
	
	if (!(ifname = malloc(BUFSIZE)))
		goto err_xit;

	tmp = fgets(ifname, BUFSIZE, f);
	bzero(ifname, BUFSIZE);
	while (fgets(ifname, BUFSIZE, f)){
		tmp = strchr(ifname, '\t');
		tmp[0] = 0;
		tmp++;
		sscanf(tmp, "%X ", &route);
		
		if (!route)
			break;
		
		bzero(ifname, BUFSIZE);
	}
	
	if (!ifname[0]){
		free(ifname);
		ifname = NULL;
	}
	
err_xit:
	fclose(f);
	return ifname;
}

// Shall be module global because used by atexit function
static const char	*pidfile = PIDFILE;
static int pidfd = -1;			// pidfile descriptor

static void
atexit_delete_pidfile(void)
{
	if (pidfd != -1) {
		unlink(pidfile);
		close(pidfd);
		pidfd = -1;
	}
}

#define LONGOPT_FOREGROUND		9000
#define LONGOPT_PIDFILE			9001


#ifndef O_CLOEXEC
# define WITH_SET_CLOEXEC 1
#endif /* O_CLOEXEC */

#ifndef SOCK_CLOEXEC
# define WITH_SET_CLOEXEC 1
#endif /* SOCK_CLOEXEC */

#ifdef WITH_SET_CLOEXEC
static int set_cloexec(int fd){
	int fd_flags;

	if ((fd_flags = fcntl(fd, F_GETFD)) < 0){
		nm_syslog(LOG_ERR, "error getting fd_flags: fcntl(): %s",
		    strerror(errno));
		return -1;
	}

	if (fcntl(fd, F_SETFD, fd_flags | FD_CLOEXEC) == -1){
		nm_syslog(LOG_ERR, "error setting close-on-exec flag: "
		    "fcntl(): %s", strerror(errno));
		return -1;
	}

	return 0;
}
#undef WITH_SET_CLOEXEC
#endif /* WITH_SET_CLOEXEC */


static int kill_modules(struct nm_module_desc_t *moddesc, int sig){
	int i;
	int ret = 1;

	for (i = 0; moddesc[i].name; i++){
		if (moddesc[i].pid){
			if (kill(moddesc[i].pid, sig)){
				moddesc[i].pid = 0;
			} else {
				ret = 0;
			}
		}
	}

	return ret;
}


static void stop_modules(struct nm_module_desc_t *moddesc){
	int retr = 20;

	while (retr){
		if (kill_modules(moddesc, SIGTERM))
			return;

		retr--;
		usleep(50000);
	}

	kill_modules(moddesc, SIGSTOP);
}


int  main(int ac, char *av[]){
	int opt, s, unused;
	FILE *f;
	char *arg, *val;
	int rand_start = 0;
	int lport = 0;
	int rport = 0;
	char *conf = NULL;
	char *mods = NULL;
	char *kmods = NULL;
#ifndef _USERLAND
	unsigned kmo_cnt = 0;
	struct kmod_opts *kmods_opts = NULL;
#endif
	struct sockaddr_in laddr;
	char *raddrstr, *laddrstr, *fmodestr, *faddrstr, *addr_if;
	struct nm_kcmd cmd_hdr;
	struct sigaction sa;
#ifdef _USERLAND
	char *ifs = NULL;
	struct nm_iface_info_t *iface_info = NULL;
	struct nm_cpu_info_t *cpu_info = NULL;
	struct nm_mem_info_t *mem_info = NULL;
	int send_dumb = 1;
#endif
	int foreground = 0;
	struct nm_hw_status_t *hwstat = NULL;
	int nohwck = 0;
	struct nm_hw_opt_t hw_opt;
	nm_server_state sst;
	int se;				// saved errno

	nm_syslog(LOG_DEBUG, "%s", "Starting");
	try_oom_adj(OOM_ENV_VARNAME);

	const struct option cmd_opts[] = {
		{"local_host",		required_argument, NULL, 'a'},
		{"local_port",		required_argument, NULL, 'p'},
		{"host",		required_argument, NULL, 'r'},
		{"port",		required_argument, NULL, 'g'},
		{"cac_mode",		required_argument, NULL, 'F'},
		{"pretend_host",	required_argument, NULL, 'f'},
		{"addr_if",		required_argument, NULL, 'j'},
		{"mods",		required_argument, NULL, 'm'},
		{"conf",		required_argument, NULL, 'c'},
		{"randomize_start", 	no_argument, NULL, 'z'},
#ifdef _USERLAND
		{"ifs",			required_argument, NULL, 'i'},
		{"no_send_cpu_mem",	no_argument, NULL, 'd'},
#endif
		{"nohwck",		no_argument, NULL, 'k'},
		{"foreground",		no_argument, NULL, LONGOPT_FOREGROUND},
		{"pidfile",		required_argument, NULL,
		    LONGOPT_PIDFILE},
		{NULL, 0, NULL, 0}
	};
	
	raddrstr = fmodestr = faddrstr = laddrstr = addr_if = NULL;
	hw_opt.retr_max = NM_HWCK_RETR_MAX;
	hw_opt.retr_interval = NM_HWCK_RETR_INTERVAL;
	// Clear sst before reading options. It's filled with some of options.
	memset(&sst, 0, sizeof(sst));
	
	//parse coomand line
	while ((opt = getopt_long_only(ac, av, "h", cmd_opts, NULL)) != -1){
		switch (opt){
		case 'a':/* address bind to */
			laddrstr = strdup(optarg);
			break;
		case 'p':/* port bind to */
			lport = atoi(optarg);
			break;
		case 'r':/* aggregator address */
			raddrstr = strdup(optarg);
			break;
		case 'g':/* aggregator port */
			rport = atoi(optarg);
			break;
		case 'F':/* control address checking mode */
			fmodestr = strdup(optarg);
			break;
		case 'f':/* pretend address */
			faddrstr = strdup(optarg);
			break;
		case 'j':
			addr_if = strdup(optarg);
			break;
		case 'c':/* configfile */
			conf = strdup(optarg);
			break;
		case 'm':/* modules to load (sensors, hybmon, smart, fs) */
			mods = strdup(optarg);
			break;
		case 'h':
			nm_usage(stdout, nm_modules);
			exit(EXIT_SUCCESS);
		case 'z':
			rand_start = 1;
			break;
#ifdef _USERLAND
		case 'd':/* don't send dumb values (cpu, mem, eth, ib) */
			send_dumb = 0;
			break;
		case 'i':/* interfaces */
			ifs = strdup(optarg);
			break;
#endif
		case 'k':/* no HW check */
			nohwck = 1;
			break;
		case LONGOPT_FOREGROUND:
			foreground = 1;
			break;
		case LONGOPT_PIDFILE:
			pidfile = strdup(optarg);
			break;
		default:
			break;
		}
	}

	hw_opt.ac = ac - optind;
	if (nohwck && hw_opt.ac){
		nm_usage(stderr, nm_modules);
		fprintf(stderr, "got wrong parameter!\n");
		nm_syslog(LOG_ERR, "%s", "got wrong parameter!");
		exit(EXIT_CONFIG);
	}
	hw_opt.av = av + optind;

	if (!conf)
		conf = CONFIGFILE;

	//parse config file
	if (conf){
		arg = malloc(BUFSIZE);
		val = malloc(BUFSIZE);
		
		if (!(arg && val))
			exit(EXIT_CONFIG);
		
		if (!(f = fopen(conf, "r"))){
			fprintf(stderr, "Config file not found\n");
			goto bad_file;
		}
		
		while (!feof(f)){
			bzero(arg, BUFSIZE);
			bzero(val, BUFSIZE);
			
			unused = fscanf(f, "%s %s", arg, val);
			
			if (!strcmp(arg, "address")){
				if (!laddrstr)
					laddrstr = strdup(val);
			} else if (!strcmp(arg, "port")){
				if (!lport)
					lport = atoi(val);
			} else if (!strcmp(arg, "randstart")){
				if (!strcmp(val, "yes"))
					rand_start = 1;
			} else if (!strcmp(arg, "if_address")){
				if (!addr_if)
					addr_if = strdup(val);
			} else if (!strcmp(arg, "modules")){
				if (!mods)
					mods = strdup(val);
			} else if (!strcmp(arg, "kmodules")){
				kmods = strdup(val);
			} else if (!strcmp(arg, "hwck_retr_max")){
				hw_opt.retr_max = atoi(val);
			} else if (!strcmp(arg, "hwck_retr_interval")){
				hw_opt.retr_interval = atoi(val);
#ifdef _USERLAND
			} else if (!strcmp(arg, "nodumb")){
				if (send_dumb && !strcmp(val, "yes"))
					send_dumb = 0;
			} else if (!strcmp(arg, "interfaces")){
				if (!ifs)
					ifs = strdup(val);
#else
			} else if (strstr(arg, "opt_") == arg){
				struct kmod_opts *tptr = realloc(kmods_opts, sizeof(struct kmod_opts) * (kmo_cnt + 1));
				if (!tptr)
					exit(EXIT_OSERR);
				kmods_opts = tptr;
				kmods_opts[kmo_cnt].kmod_name = strdup(arg);
				kmods_opts[kmo_cnt].kmod_opt = strdup(val);
				kmo_cnt++;
#endif
			} else if (!strcmp(arg, "") || arg[0] == '#'){
				//Do nothing - comment or empty string
			} else {
				fprintf(stderr, "Bad config file key: %s\n", arg);
				fclose(f);
bad_file:
				nm_syslog(LOG_ERR, "%s", "bad configuration file");
				exit(EXIT_CONFIG);
			}
		}
		
		fclose(f);
		free(val);
		free(arg);
		if (conf != (char *) CONFIGFILE)
			free(conf);
	}

	if ((raddrstr && !rport) || (rport && !raddrstr)){
		fprintf(stderr, "Remote host or remote port not specified!\n");
		nm_syslog(LOG_ERR, "%s", "bad configuration - remote address");
		exit(EXIT_CONFIG);
	}

	if (rport > 65535 || lport > 65535){
		fprintf(stderr, "Bad port value!\n");
		nm_syslog(LOG_ERR, "%s", "bad configuration - local port");
		exit(EXIT_CONFIG);
	}

	sst.cac_mode = CAC_MODE_IFACE;
	sst.ctladdr_fixed = 0;		// allow to change

	if (fmodestr) {
		char *p = strtok(fmodestr, ",");
		while (p != NULL) {
			if (!strcmp(p, "iface"))
				sst.cac_mode = CAC_MODE_IFACE;
			else if (!strcmp(p, "any"))
				sst.cac_mode = CAC_MODE_ANY;
			else if (!strcmp(p, "exact"))
				sst.cac_mode = CAC_MODE_EXACT;
			else if (!strcmp(p, "fixed"))
				sst.ctladdr_fixed = 1;
			else if (!strcmp(p, "free"))
				sst.ctladdr_fixed = 0;
			else {
				fprintf(stderr, "Bad cac_mode!\n");
				nm_syslog(LOG_ERR, "bad cac_mode");
				exit(EXIT_CONFIG);
			}
			p = strtok(NULL, ",");
		}
	}
	
	if (faddrstr && addr_if){
		fprintf(stderr, "Only pretend_host or addr_if should be specified, but not both!\n");
		nm_syslog(LOG_ERR, "conflicting address parameters");
		exit(EXIT_CONFIG);
	}

	if (!faddrstr && !addr_if &&
	    (sst.cac_mode == CAC_MODE_EXACT || sst.ctladdr_fixed))
	{
		fprintf(stderr, "No address with exact/fixed cac_mode!\n");
		nm_syslog(LOG_ERR, "conflicting address parameters");
		exit(EXIT_CONFIG);
	}

	if (hw_opt.retr_max < 0 || hw_opt.retr_max > 255){
		fprintf(stderr, "Bad parameter value - hwck_retr_max=%d out of range from 0 to 255.\n",
			hw_opt.retr_max);
		nm_syslog(LOG_ERR, "Bad parameter value - hwck_retr_max=%d out of range from 0 to 255",
			hw_opt.retr_max);
		exit(EXIT_CONFIG);
	}

	if (hw_opt.retr_interval < 0 || hw_opt.retr_interval > 255){
		fprintf(stderr, "Bad parameter value - hwck_retr_interval=%d out of range from 0 to 255.\n",
			hw_opt.retr_interval);
		nm_syslog(LOG_ERR, "Bad parameter value - hwck_retr_interval=%d out of range from 0 to 255",
			hw_opt.retr_interval);
		exit(EXIT_CONFIG);
	}

	bzero(&laddr, sizeof(struct sockaddr_in));
	bzero(&cmd_hdr, sizeof(cmd_hdr));

	laddr.sin_family = AF_INET;
	if (laddrstr){
		if (nm_getaddr(laddrstr, &laddr.sin_addr)){
			fprintf(stderr, "Bad address: %s!\n", laddrstr);
			nm_syslog(LOG_ERR, "%s", "bad local address");
			exit(EXIT_CONFIG);
		}
		laddr.sin_family = AF_INET;
		laddr.sin_port = htons(lport);
		free(laddrstr);
	}
	if (lport){
		laddr.sin_port = htons(lport);
	}

	if (raddrstr){
		if (nm_getaddr(raddrstr, &(cmd_hdr.tgt_host))){
			fprintf(stderr, "Bad remote address: %s!\n", raddrstr);
			nm_syslog(LOG_ERR, "%s", "bad remote address");
			exit(EXIT_CONFIG);
		}
		cmd_hdr.tgt_port = htons(rport);
		free(raddrstr);
	}


	if (faddrstr){
		if (nm_getaddr(faddrstr, &(cmd_hdr.client_host))){
			fprintf(stderr, "Bad fake address: %s!\n", faddrstr);
			nm_syslog(LOG_ERR, "%s", "bad fake address");
			exit(EXIT_CONFIG);
		}
		free(faddrstr);
	} else {
		if (addr_if){
			cmd_hdr.client_host.b4[0] = nm_get_addr_if(addr_if);
			free(addr_if);
		} else if (laddr.sin_addr.s_addr == INADDR_ANY){
			if ((addr_if = nm_get_default_route_if())){
				cmd_hdr.client_host.b4[0] = nm_get_addr_if(addr_if);
				free(addr_if);
			} else {
				cmd_hdr.client_host.b4[0] = htonl(INADDR_LOOPBACK);
			}
		} else {
			memcpy(&(cmd_hdr.client_host), &(laddr.sin_addr.s_addr), sizeof(uint32_t));
		}
	}

	//Modules pre-initialization
	if (mods){
		nm_syslog(LOG_DEBUG, "%s", "local modules preinitialization");
		if (preinit_modules(nm_modules, mods) < 0){
			fprintf(stderr, "Cannot init modules!\n");
			nm_syslog(LOG_ERR, "%s", "local modules preinit failed");
			exit(EXIT_CONFIG);
		}
		free(mods);
	}

#ifdef _USERLAND
	//Interfaces check and initialization
	if (ifs){
		nm_syslog(LOG_DEBUG, "%s", "init interfaces data");
		iface_info = malloc(sizeof(struct nm_iface_info_t));
		if (!iface_info){
			fprintf(stderr, "Memory allocation failed!\n");
			nm_syslog(LOG_ERR, "%s", "ENOMEM");
			exit(EXIT_MEMORY);
		}
		bzero(iface_info, sizeof(struct nm_iface_info_t));
		if (nm_init_ifaces(ifs, iface_info) < 0){
			se = errno;
			fprintf(stderr, "Cannot init interfaces data!\n");
			nm_syslog(LOG_ERR, "Interfaces init failed: %s",
				strerror(errno));
			exit(errno_to_exitcode(se));
		}
		free(ifs);
	}
	
	if (send_dumb){
		nm_syslog(LOG_DEBUG, "%s", "init dumb data");
		cpu_info = malloc(sizeof(struct nm_cpu_info_t));
		mem_info = malloc(sizeof(struct nm_mem_info_t));
		if (!(mem_info && cpu_info)){
			fprintf(stderr, "Memory allocation failed!\n");
			nm_syslog(LOG_ERR, "memory allocation failed");
			exit(EXIT_MEMORY);
		}
		bzero(cpu_info, sizeof(struct nm_cpu_info_t));
		bzero(mem_info, sizeof(struct nm_mem_info_t));
		if (nm_init_cpus(cpu_info)){
			se = errno;
			fprintf(stderr, "Cannot init cpu data: %s!\n",
			    strerror(se));
			nm_syslog(LOG_ERR, "error init CPU data: %s",
			    strerror(se));
			exit(errno_to_exitcode(se));
		}
		if (nm_init_mem(mem_info)){
			se = errno;
			fprintf(stderr, "Cannot init memory data: %s!\n",
			    strerror(se));
			nm_syslog(LOG_ERR, "error init memory data: %s",
			    strerror(se));
			exit(errno_to_exitcode(se));
		}
	}
#endif

	if (!nohwck){
		hwstat = malloc(sizeof(struct nm_hw_status_t));
		if (!hwstat){
			fprintf(stderr, "Memory allocation failed!\n");
			nm_syslog(LOG_ERR, "%s", "Memory allocation failed!");
			exit(EXIT_MEMORY);
		}
		bzero(hwstat, sizeof(struct nm_hw_status_t));
	}

	if (!foreground) {
		nm_syslog(LOG_NOTICE, "%s", "daemonizing...");
		if (daemon(0,0) < 0) {
			return 1;
		}
	}

	setpgid(getpid(), getpid());

	if (0 != strcmp(pidfile, NO_PIDFILE)) {
		char pidbuf[13];	// signed 32 bits plus \n plus \0
		int pidfd_flags = O_RDWR | O_CREAT;
#ifdef O_CLOEXEC
		pidfd_flags |= O_CLOEXEC;
#endif
		pidfd = open(pidfile, pidfd_flags, 0600);
		if (pidfd == -1) {
			nm_syslog(LOG_ERR, "error opening pidfile: %s",
			    strerror(errno));
			exit(EXIT_NOPERM);
		}
#ifndef O_CLOEXEC
		if (set_cloexec(pidfd)){
			nm_syslog(LOG_ERR, "cannot set close-on-exec flag "
			    "for the pidfile");
			exit(EXIT_OSERR);
		}
#endif
		if (flock(pidfd, LOCK_EX | LOCK_NB) != 0) {
			nm_syslog(LOG_ERR, "error locking pidfile: %s",
			    strerror(errno));
			exit(EXIT_NOPERM);
		}
		snprintf(pidbuf, sizeof(pidbuf), "%ld\n", (long)getpid());
		size_t pblen = strlen(pidbuf);
		errno = 0;
		if ((ssize_t)pblen != write(pidfd, pidbuf, pblen)) {
			nm_syslog(LOG_ERR, "error writing pidfile: %s",
			    strerror(errno));
			exit(EXIT_OSERR);
		}
	}
	atexit(atexit_delete_pidfile);

	if (!nohwck){
		if (nm_init_hwstat(&hw_opt, hwstat) < 0){
			se = errno;
			nm_syslog(LOG_ERR, "error init hwstat data: %s",
			    strerror(se));
			exit(errno_to_exitcode(se));
		}
	}

	//setup signals
	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, SIG_IGN);

	bzero(&sa, sizeof(struct sigaction));
	sa.sa_sigaction = nm_handlechild;
	sa.sa_flags = SA_NOCLDSTOP | SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGCHLD, &sa, NULL);
	
	bzero(&sa, sizeof(struct sigaction));
	sa.sa_handler = nm_term;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGTERM, &sa, NULL);

	int socket_type = SOCK_DGRAM;
#ifdef SOCK_CLOEXEC
	socket_type |= SOCK_CLOEXEC;
#endif
	if ((s = socket(PF_INET, socket_type, IPPROTO_UDP)) < 0){
		nm_syslog(LOG_ERR, "socket(): %s", strerror(errno));
		exit(EXIT_OSERR);
	}
#ifndef SOCK_CLOEXEC
	if (set_cloexec(s)){
		nm_syslog(LOG_ERR, "cannot set close-on-exec flag "
		    "for the socket");
		exit(EXIT_OSERR);
	}
#endif
	if (bind(s, (struct sockaddr *)&laddr, sizeof(struct sockaddr_in))){
		nm_syslog(LOG_ERR, "bind(): %s", strerror(errno));
		exit(EXIT_NOPERM);
	}
	
	//Load required kernel modules
#ifdef _USERLAND
	if (kmods){
		nm_syslog(LOG_NOTICE, "%s", "loading kernel modules");
		load_kmods(kmods);
		free(kmods);
	}
#else
	if (kmods_opts){
		struct kmod_opts *tptr = realloc(kmods_opts, sizeof(struct kmod_opts) * (kmo_cnt + 1));
		kmods_opts = tptr;
		kmods_opts[kmo_cnt].kmod_name = NULL;
		kmods_opts[kmo_cnt].kmod_opt = NULL;
	}
	nm_syslog(LOG_NOTICE, "%s", "loading kernel modules");
	load_kmods(kmods, kmods_opts, hwstat);
	//Cleanup
	for (unused = 0; unused < kmo_cnt; unused++){
		//A little bit paranoja
		if (kmods_opts[unused].kmod_name)
			free(kmods_opts[unused].kmod_name);
		else break;
		if (kmods_opts[unused].kmod_opt)
			free(kmods_opts[unused].kmod_opt);
	}
	free(kmods_opts);
#endif

	sst.sock = s;
	sst.cmd_hdr = &cmd_hdr;
	nm_srand(&sst);

	// Calm Before the Storm;)
	// This randomization is needed to avoid synchronous sending of
	// data packets by thousands of agents and then overflowing an
	// aggregator incoming interfaces.
	if (rand_start)
		usleep(rand() % 1000000);
	
	//Starting modules
//TODO: error handling
	nm_syslog(LOG_INFO, "%s", "Starting modules");
	start_modules(nm_modules);
	
	nm_syslog(LOG_NOTICE, "%s", "Starting service");

#ifdef _USERLAND
	sst.cpu_info = cpu_info;
	sst.mem_info = mem_info;
	sst.iface_info = iface_info;
	sst.hwstat = hwstat;
#endif
	nm_server(&sst);
	nm_syslog(LOG_NOTICE, "%s", "Stopping service");
#ifndef _USERLAND
	unload_kmods(kmods);
	if (kmods)
		free(kmods);
#else
	nm_clean_modules();
	nm_release_ifaces(iface_info);
	
	if (send_dumb){
		nm_release_mem(mem_info);
		nm_release_cpus(cpu_info);
		free(mem_info);
		free(cpu_info);
	}
#endif
	if(hwstat){
		nm_cleanup_hwstat(hwstat);
		free(hwstat);
	}
	
	//Final routines
	stop_modules(nm_modules);
	close(s);

	return 0;
}


