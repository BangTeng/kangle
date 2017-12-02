#ifndef KCNAME_H_lksjfds
#define KCNAME_H_lksjfds
#include "time_utils.h"
#include "global.h"
#ifdef ENABLE_CNAME_BIND
class KHttpRequest;
typedef void (*cname_call_back)(KHttpRequest *rq,const char *cname,int cname_size);
void kgl_find_cname(const char *hostname,cname_call_back cb,KHttpRequest *rq);
int kgl_find_cache_cname(const char *hostname,char *cname,int cname_size);
void init_cname_worker();
void flush_cname_cache(time_t nowTime);
#endif
#endif
