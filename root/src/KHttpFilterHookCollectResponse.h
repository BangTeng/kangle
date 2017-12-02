#ifndef KHTTPFILTERHOOKCOLLECTRESPONSE_H
#define KHTTPFILTERHOOKCOLLECTRESPONSE_H
#include "KHttpFilterHook.h"
#ifdef ENABLE_KSAPI_FILTER
class KHttpFilterHookCollectResponse : public KHttpFilterHookCollect
{
public:
	int check_response(KHttpRequest *rq);
};
#endif
#endif
