#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <plugins_api.h>


#define NFSD_STAT_FILE	"/proc/net/rpc/nfsd"


void nfsd_process(char *param, int fd){
	short param_found = 0;
	char line[BUFSIZE];
	char *pline;
	char *answ;
	FILE *f;

	if (!(f = fopen(NFSD_STAT_FILE, "r"))){
		hmp_write_error(HMP_SRC_ERR_SYSTEM, errno, fd);
		return;
	}

	while (fgets(line, sizeof(line), f)){
		if (!(pline = strchr(line, ' '))){
			hmp_write_error(HMP_SRC_ERR_SYSTEM, ENOENT, fd);
			goto close_stat_file;
		}
		*pline = '\0';
		answ = ++pline;

		if (!strcmp(line, param)){
			param_found = 1;
			break;
		}
	}

	if (!param_found){
		hmp_write_error(HMP_SRC_ERR_SYSTEM, ENOENT, fd);
		goto close_stat_file;
	}

	if ((pline = strchr(answ, '\n'))){
		*pline = '\0';
	}
	hmp_write_local_answer(param, answ, fd);

close_stat_file:
	fclose(f);
}

