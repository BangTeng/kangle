#include <stdio.h> 
#include <stdarg.h>
#ifndef _WIN32
#include <syslog.h>
#include <pthread.h>
#include <signal.h>
#endif
#include "klog.h"
static kgl_vklog_callback vklog_f = NULL;

void klog_init(kgl_vklog_callback kgl_vklog)
{
	vklog_f = kgl_vklog;
}
void vklog(int level, const char *fmt, va_list ap) {
	if (vklog_f) {
		vklog_f(level, fmt, ap);
	} else {
		vprintf(fmt, ap);
	}
}
void debug(const char *fmt, ...) {
#ifndef NDEBUG	
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
#endif
}
void klog(int level, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vklog(level, fmt, ap);
	va_end(ap);
}
