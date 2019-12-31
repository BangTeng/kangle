#ifndef KDSOASYNCFETCHOBJECT_H
#define KDSOASYNCFETCHOBJECT_H
#include "KFetchObject.h"
#include "KHttpResponseParser.h"

class KDsoAsyncFetchObject : public KFetchObject
{
public:
	KDsoAsyncFetchObject()
	{
		memset(&ctx, 0, sizeof(ctx));
	}
	KDsoAsyncFetchObject(void *model_ctx)
	{
		memset(&ctx, 0, sizeof(ctx));
		ctx.model_ctx = model_ctx;
	}
	bool NeedTempFile(bool upload, KHttpRequest *rq)
	{
		if (upload) {
			return rq->content_length == -1;
		}
		return false;
	}
#ifdef ENABLE_REQUEST_QUEUE
	bool needQueue()
	{
		if (next) {
			return next->needQueue();
		}
		return false;
	}
#endif
	~KDsoAsyncFetchObject();
	kev_result open(KHttpRequest *rq);
	void close(KHttpRequest *rq);
	kev_result start_response_body(KHttpRequest *rq);
	kev_result readBody(KHttpRequest *rq);
	kev_result ReadPost(KHttpRequest *rq,LPVOID lpvBuffer, DWORD lpdwSize);
	kev_result ReadPostCallback(KHttpRequest *rq,int got);
	KGL_RESULT ResponseHeader(KHttpRequest *rq, const char *attr, hlen_t attr_len, const char *val, hlen_t val_len);
	WSABUF up_buffer;
private:
	kgl_async_context ctx;
	KHttpResponseParser parser_ctx;
};
#endif

