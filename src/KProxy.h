#ifndef KPROXY_H_ASDF0
#define KPROXY_H_ASDF0
#include "global.h"
#include "kconnection.h"
#ifdef ENABLE_PROXY_PROTOCOL
class KHttpRequest;
class KReadWriteBuffer;
kev_result handl_proxy_request(kconnection *cn,result_callback cb);
bool build_proxy_header(KReadWriteBuffer *buffer, KHttpRequest *rq);
#endif
#endif
