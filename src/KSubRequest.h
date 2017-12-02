#ifndef KSUBREQUEST_H
#define KSUBREQUEST_H
#include "KHttpRequest.h"
#include "KFetchObject.h"
#include "KFileName.h"
#include "KContext.h"
#include "malloc_debug.h"

/*
sub request context use in ssi.
*/
class KSubRequest
{
public:
	KSubRequest()
	{
		memset(this,0,sizeof(KSubRequest));
	}
	void freeBack(KHttpRequest *rq)
	{
		if (data && callBack) {
			callBack(rq,data,sub_request_free);
		}
	}
	void destroy(KHttpRequest *rq)
	{
		freeBack(rq);
		if (file) {
			delete file;
		}
		if (fetchObj) {
			delete fetchObj;
		}
		if (url) {
			delete url;
		}
		if (ctx) {
			ctx->clean_obj(rq);
			ctx->clean();
			delete ctx;
		}
	}
	sub_request_call_back callBack;
	void *data;
	KFileName *file;
	KFetchObject *fetchObj;
	KUrl *url;
	KContext *ctx;
	char meth;
	int flags;
	KSubRequest *next;
};
#endif
