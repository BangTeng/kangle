#ifndef KACCESSDSOSUPPORT_H
#define KACCESSDSOSUPPORT_H
#include "ksapi.h"
#include "KAutoBuffer.h"
#include "KHttpRequest.h"
void init_access_dso_support(kgl_access_context *ctx,int notify);
KGL_RESULT add_api_var(LPVOID buffer, LPDWORD size, const char *val, int len = 0);
KGL_RESULT get_request_variable(KHttpRequest *rq, KGL_VAR type, LPSTR  name, LPVOID  buffer, LPDWORD size);
class KAccessRequest
{
public:
	KAccessRequest(KHttpRequest *rq)
	{
		this->rq = rq;
		buffer = NULL;
	}
	~KAccessRequest()
	{
		if (buffer) {
			delete buffer;
		}
	}
	KAutoBuffer *GetBuffer()
	{
		if (buffer == NULL) {
			buffer = new KAutoBuffer(rq->pool);
		}
		return buffer;
	}
	KHttpRequest *rq;
	KAutoBuffer *buffer;
};
#endif
