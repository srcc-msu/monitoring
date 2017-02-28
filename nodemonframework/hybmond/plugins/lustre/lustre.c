#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <plugins_api.h>
#include "lustre_defs.h"


void PLUGIN_PROCESS(char *param, int fd){
	int cfd, unused;
	char *tmp;
	char *name;

	name = malloc(BUFSIZE);
	sprintf(name, LUSTRE_PREFIX "%s", param);
	tmp = name;

	while ((tmp = strchr(tmp, '.')))
		tmp[0] = '/';

	cfd = open(name, O_RDONLY);
	if (cfd < 0){
		hmp_write_error(HMP_SRC_ERR_SYSTEM, errno, fd);
		goto err_xit;
	}
	unused = read(cfd, name, BUFSIZE);
	close(cfd);
	
	name[unused] = 0;
	hmp_write_local_answer(param, name, fd);
err_xit:
	free(name);
}


