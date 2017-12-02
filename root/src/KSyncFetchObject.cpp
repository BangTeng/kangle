#include "KSyncFetchObject.h"
#include "http.h"
#include "KSelector.h"
void KSyncFetchObject::open(KHttpRequest *rq)
{
	kassert(TEST(rq->flags,RQ_SYNC));
#ifdef ENABLE_HTTP2
	if (rq->http2_ctx == NULL) {
#endif
		rq->c->socket->set_time(conf.time_out);
#ifndef _WIN32
		rq->c->socket->setblock();
#else
		if (TEST(rq->workModel, WORK_MODEL_SSL)) {
			rq->c->socket->setblock();
		}
#endif
#ifdef ENABLE_HTTP2
	}
#endif
	process(rq);
#ifdef ENABLE_HTTP2
	if (rq->http2_ctx == NULL) {
#endif
#ifndef _WIN32
		rq->c->socket->setnoblock();
#else
		if (TEST(rq->workModel,WORK_MODEL_SSL)) {
			rq->c->socket->setnoblock();
		}
#endif
#ifdef ENABLE_HTTP2
	}
#endif
	CLR(rq->flags,RQ_SYNC);
	stageEndRequest(rq);
}
