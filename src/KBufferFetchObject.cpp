#include "KBufferFetchObject.h"
#include "http.h"
static int buffer_fetch_object(void *arg,LPWSABUF buffer, int bc)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KBufferFetchObject *fo = static_cast<KBufferFetchObject *>(rq->fetchObj);
	return kr_get_read_buffers(&fo->buffer, buffer, bc);
}
kev_result result_fetch_object(void *arg, int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	if (got <= 0) {
		return stageEndRequest(rq);
	}
	rq->addFlow(got);
	KBufferFetchObject *fo = static_cast<KBufferFetchObject *>(rq->fetchObj);
	if (kr_read_success(&fo->buffer, got)) {
		return rq->Write(rq, result_fetch_object, buffer_fetch_object);
	}
	rq->ctx->expected_done = 1;
	return stageEndRequest(rq);
}
kev_result KBufferFetchObject::open(KHttpRequest *rq)
{
	return rq->Write(rq, result_fetch_object, buffer_fetch_object);
}
