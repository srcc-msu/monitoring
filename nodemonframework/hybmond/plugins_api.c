#define _GNU_SOURCE

#include <errno.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "plugin_err.h"
#include "plugins_api.h"
#include "proto_err.h"
#include "urlcode.h"


void hmp_write_error(int source, int code, int fd){
	ssize_t unused;
	char *buf;
	char *tmp = NULL;

	switch (source){
	case HMP_SRC_ERR_SYSTEM:
		unused = asprintf(&buf, "_error=system,%d,%s\r\n", code, (tmp = url_encode(strerror(code))));
		break;
	case HMP_SRC_ERR_PLUGIN:
		unused = asprintf(&buf, "_error=plugin,%d,%s\r\n", code, (tmp = url_encode(hmp_plugin_errstr(code))));
		break;
	case HMP_SRC_ERR_PROTO:
		unused = asprintf(&buf, "%d %s\r\n", code, hmp_proto_errstr(code));
		break;
	default:
		return;
	}

	if (tmp)
		free(tmp);

	unused = write(fd, buf, strlen(buf));
	free(buf);
}


void hmp_write_answer(char *prefix, char *param, char *answer, int fd){
	char *tmp;
	char *buf;
	int unused;
	
	tmp = url_encode(answer);
	unused = asprintf(&buf, "%s.%s=%s\r\n", prefix, param, tmp);
	free(tmp);
	
	unused = write(fd, buf, strlen(buf));
	free(buf);
}


