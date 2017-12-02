#include "KContentTransfer.h"
#include "KChunked.h"
#include "malloc_debug.h"
#include "KGzip.h"
#include "KFilterContext.h"

bool KContentTransfer::init(KHttpRequest *rq)
{
	this->rq = rq;
	return true;
}
StreamState KContentTransfer::write_all(const char *str,int len)
{
	assert(rq->of_ctx);
	if(rq->of_ctx->checkFilter(rq,str,len)==JUMP_DENY){
		SET(rq->filter_flags,RQ_RESPONSE_DENY);
		SET(rq->flags,RQ_CONNECTION_CLOSE);
		return STREAM_WRITE_FAILED;
	}
	return KHttpStream::write_all(str,len);
}
