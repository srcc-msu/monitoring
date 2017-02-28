#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/utsname.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <plugins_api.h>
#include <versions.h>


#define UPTIME_FILE	"/proc/uptime"


static void hmp_systm_process_descr(char *param, int fd){
	int unused;
	struct utsname descr;
	char *buf;

	if (uname(&descr) < 0){
		hmp_write_error(HMP_SRC_ERR_SYSTEM, errno, fd);
		return;
	}

	unused = asprintf(&buf, "%s %s %s", descr.sysname, descr.release, descr.machine);

	hmp_write_local_answer(param, buf, fd);
	free(buf);
}


static void hmp_systm_process_uptime(char *param, int fd){
	int fdc, unused;
	char *tmp;
	char *uptime;

	if ((fdc = open(UPTIME_FILE, O_RDONLY)) < 0){
		hmp_write_error(HMP_SRC_ERR_SYSTEM, errno, fd);
		return;
	}
	uptime = malloc(BUFSIZE);
	unused = read(fdc, uptime, BUFSIZE);
	close(fdc);
	tmp = strchr(uptime, '.');
	tmp[0] = 0;

	hmp_write_local_answer(param, uptime, fd);
	free(uptime);
}


static void hmp_systm_process_name(char *param, int fd){
	char *name = malloc(BUFSIZE);

	if (gethostname(name, BUFSIZE) < 0){
		hmp_write_error(HMP_SRC_ERR_SYSTEM, errno, fd);
		goto err_xit;
	}

	hmp_write_local_answer(param, name, fd);
err_xit:
	free(name);
}


static void hmp_systm_process_agent_version(char *param, int fd){
	char *ver;
	int unused;

	unused = asprintf(&ver, "%s", HM_AGENT_VERSION_STR);

	hmp_write_local_answer(param, ver, fd);
	free(ver);
}


void system_process(char *param, int fd){
	int i = 0;

	const struct {
		char *name;
		void (*proc_func)(char *, int);
	} system_param[] = {
		{"description", hmp_systm_process_descr},
		{"uptime", hmp_systm_process_uptime},
		{"name", hmp_systm_process_name},
		{"agent_version", hmp_systm_process_agent_version},
		{NULL, NULL}
	};

	while (system_param[i].name){
		if (!strcmp(system_param[i].name, param)){
			system_param[i].proc_func(param, fd);
			return;
		}
		i++;
	}

	hmp_write_error(HMP_SRC_ERR_SYSTEM, ENOENT, fd);
}

