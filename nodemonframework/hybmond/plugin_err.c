#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>

#include "plugin_err.h"

#define HM_PLUGIN_ERR_OK_STR		"OK"
#define HM_PLUGIN_ERR_INVALID_STR	"invalid_variable"
#define HM_PLUGIN_ERR_NOT_FOUND_STR	"unknown_plugin"

static char *hmp_plugin_errs[] = {
	/* 0 HM_PLUGIN_ERR_OK_STR */		HM_PLUGIN_ERR_OK_STR,
	/* 1 HM_PLUGIN_ERR_INVALID_STR */	HM_PLUGIN_ERR_INVALID_STR,
	/* 2 HM_PLUGIN_ERR_NOT_FOUND_STR*/	HM_PLUGIN_ERR_NOT_FOUND_STR
};

char *hmp_plugin_errstr(int code){
	if (code > HM_PLUGIN_MAX_ERRS)
		return NULL;
	return hmp_plugin_errs[code];
}

