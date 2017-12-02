#ifndef KLOGDRILL_H
#define KLOGDRILL_H
#include "global.h"
#ifdef ENABLE_LOG_DRILL
#include "KString.h"
struct klog_drill {
	char *buf;
	int len;
	klog_drill *next;
	klog_drill *prev;
};
class KHttpRequest;
void add_log_drill(KHttpRequest *rq,KStringBuf &s);
void flush_log_drill();
void init_log_drill();
#endif
#endif

