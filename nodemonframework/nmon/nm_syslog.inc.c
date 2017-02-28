#include <nm_syslog.h>
#include <syslog.h>
#include <stdarg.h>

void nm_syslog(int prio, const char *fmt, ...)
{
	va_list ap;

	openlog(APPNAME, 0, LOG_DAEMON);
	va_start(ap, fmt);
	vsyslog(prio, fmt, ap);
	va_end(ap);
	closelog();
}

void nm_vsyslog(int prio, const char *fmt, va_list ap)
{
	openlog(APPNAME, 0, LOG_DAEMON);
	vsyslog(prio, fmt, ap);
	closelog();
}
