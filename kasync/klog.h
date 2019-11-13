#ifndef KLOG_H_23541234123413241234
#define KLOG_H_23541234123413241234
#include <time.h>
#include <stdarg.h>
#include "kfeature.h"
#define KLOG_ERR			1
#define KLOG_WARNING		2
#define KLOG_NOTICE			3
#define KLOG_INFO			4
#define KLOG_DEBUG			5
KBEGIN_DECLS
void vklog(int level, const char *fmt, va_list ap);
typedef void (*kgl_vklog_callback)(int level, const char *fmt, va_list ap);
void klog(int level, const char *fmt, ...);
void klog_init(kgl_vklog_callback kgl_vklog);
void CTIME_R(time_t *a, char *b, size_t l);
void debug(const char *fmt, ...);
KEND_DECLS
#endif
