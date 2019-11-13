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
	~KDsoAsyncFetchObject();
	kev_result open(KHttpRequest *rq);
	kev_result start_response_body(KHttpRequest *rq);
	kev_result readBody(KHttpRequest *rq);
	KGL_RESULT response_header(KHttpRequest *rq, const char *attr, hlen_t attr_len, const char *val, hlen_t val_len);
private:
	kgl_async_context ctx;
	KHttpResponseParser parser_ctx;
};
#endif

