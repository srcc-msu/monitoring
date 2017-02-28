#ifndef _HM_PLUGINS_API_H_
#define _HM_PLUGINS_API_H_

#include <unistd.h>


enum {
	HMP_SRC_ERR_SYSTEM,
	HMP_SRC_ERR_PLUGIN,
	HMP_SRC_ERR_PROTO
};


void hmp_write_error(int, int, int);
void hmp_write_answer(char *, char *, char *, int);


/* LOCAL PREFIX _MUST_ be defined in plugin */
#define hmp_write_local_answer(param, answer,fd)	hmp_write_answer(LOCAL_PREFIX, param, answer, fd)

#define BUFSIZE		sysconf(_SC_PAGESIZE)


#endif

