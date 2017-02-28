#ifndef _NM_SYSLOG_H_
#define _NM_SYSLOG_H_

#include <stdarg.h>
#include <syslog.h>

void nm_syslog(int prio, const char *fmt, ...);
void nm_vsyslog(int prio, const char *fmt, va_list ap);

#endif
