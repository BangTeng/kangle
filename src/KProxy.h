#ifndef KPROXY_H_ASDF0
#define KPROXY_H_ASDF0
#include "global.h"
#include "KSocketBuffer.h"
#ifdef ENABLE_PROXY_PROTOCOL
class KHttpRequest;
void handl_proxy_request(KHttpRequest *rq);
bool build_proxy_header(KSocketBuffer *buffer, KHttpRequest *rq);
#endif
#endif
