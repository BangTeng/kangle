#include "KSyncFetchObject.h"
#include "http.h"
#include "kselector.h"
kev_result sync_next_request(void *arg, int len)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	return stageEndRequest(rq);
}
kev_result KSyncFetchObject::open(KHttpRequest *rq)
{
	kassert(TEST(rq->flags,RQ_SYNC));
	process(rq);
	kgl_selector_module.next(rq->sink->GetSelector(),sync_next_request, rq,0);
	return kev_ok;
}
