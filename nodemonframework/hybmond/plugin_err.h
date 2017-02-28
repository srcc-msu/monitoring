#ifndef _HM_PLUGIN_ERR_H_
#define _HM_PLUGIN_ERR_H_

#define HM_PLUGIN_ERR_OK		0
#define HM_PLUGIN_ERR_INVALID		1
#define HM_PLUGIN_ERR_NOT_FOUND		2

#define HM_PLUGIN_MAX_ERRS		2


char *hmp_plugin_errstr(int code);


#endif
