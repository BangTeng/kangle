#ifndef KHTTPFILTERHOOKCOLLECTREQUEST_H
#define KHTTPFILTERHOOKCOLLECTREQUEST_H
#include "KHttpFilterHook.h"
#ifdef ENABLE_KSAPI_FILTER
class KHttpFilterHookCollectRequest : public KHttpFilterHookCollect
{
public:
	int check_request(KHttpRequest *rq);
};
#endif
#endif
