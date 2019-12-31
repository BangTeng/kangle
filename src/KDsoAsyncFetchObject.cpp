#include "KDsoAsyncFetchObject.h"
#include "KAccessDsoSupport.h"
#include "KDsoRedirect.h"
#include "http.h"

static KGL_RESULT push_data(KCONN cn,LPVOID buf,DWORD len)
{
	KHttpRequest *rq = (KHttpRequest *)cn;
	KDsoAsyncFetchObject *fo = static_cast<KDsoAsyncFetchObject *>(rq->fetchObj);
	switch (fo->PushBody(rq, (const char *)buf, len)) {
	case STREAM_WRITE_SUCCESS:
		return KGL_OK;
	default:
		return KGL_EUNKNOW;
	}
}
static kev_result next_upstream(KCONN cn,void *queue)
{
	return ((KHttpRequest *)cn)->NextFetchObject((KRequestQueue *)queue);
}
static kev_result try_write(KCONN  cn)
{
	return try_send_request((KHttpRequest *)cn);
}
static kev_result read_post(KCONN cn, LPVOID lpvBuffer,DWORD lpdwSize)
{
	KHttpRequest *rq = (KHttpRequest *)cn;
	KDsoAsyncFetchObject *fo = static_cast<KDsoAsyncFetchObject *>(rq->fetchObj);
	return fo->ReadPost(rq, lpvBuffer, lpdwSize);
}
static kev_result start_response_body(KCONN cn)
{
	KHttpRequest *rq = (KHttpRequest *)cn;
	KDsoAsyncFetchObject *fo = static_cast<KDsoAsyncFetchObject *>(rq->fetchObj);
	return fo->start_response_body(rq);
}
static kev_result end_request(KCONN cn, BOOL expected_end)
{
	return stage_rdata_end((KHttpRequest *)cn, expected_end ? STREAM_WRITE_SUCCESS : STREAM_WRITE_FAILED);
}
static KGL_RESULT response_unknow_header(KCONN cn, const char *attr, hlen_t attr_len, const char *val, hlen_t val_len)
{
	KHttpRequest *rq = (KHttpRequest *)cn;
	KDsoAsyncFetchObject *fo = static_cast<KDsoAsyncFetchObject *>(rq->fetchObj);
	return fo->ResponseHeader(rq, attr, attr_len, val, val_len);
}
static KGL_RESULT support_function (
	KCONN      cn,
	DWORD      dwHSERequest,
	LPVOID     lpvBuffer,
	LPDWORD    lpdwSize,
	LPDWORD    lpdwDataType)
{
	return KGL_ENOT_SUPPORT;
}
static int buffer_read_post(void *arg, LPWSABUF buf, int bc) {
	KHttpRequest *rq = (KHttpRequest *)arg;
	KDsoAsyncFetchObject *fo = static_cast<KDsoAsyncFetchObject *>(rq->fetchObj);
	buf[0].iov_base = fo->up_buffer.iov_base;
	buf[0].iov_len = fo->up_buffer.iov_len;
	return 1;
}
static kev_result result_read_post(void *arg, int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KDsoAsyncFetchObject *fo = static_cast<KDsoAsyncFetchObject *>(rq->fetchObj);
	return fo->ReadPostCallback(rq,got);
}
static kgl_async_context async_context = {
	sizeof(kgl_async_context),
	0,
	NULL,
	NULL,
	(kgl_get_variable_f)get_request_variable,
	response_unknow_header,
	NULL,
	start_response_body,
	push_data,
	next_upstream,
	try_write,
	read_post,
	end_request,
	support_function
};
kev_result KDsoAsyncFetchObject::ReadPost(KHttpRequest *rq,LPVOID lpvBuffer, DWORD lpdwSize)
{
	if (rq->left_read <= 0) {
		return ReadPostCallback(rq, 0);
	}
	if ((INT64)lpdwSize > rq->left_read) {
		lpdwSize = (DWORD)rq->left_read;
	}
	up_buffer.iov_base = (char *)lpvBuffer;
	up_buffer.iov_len = (int)lpdwSize;
	return rq->Read(rq, result_read_post, buffer_read_post);
}
KDsoAsyncFetchObject::~KDsoAsyncFetchObject()
{
	if (ctx.model_ctx) {
		kassert(brd!=NULL);
		KDsoRedirect *dr = static_cast<KDsoRedirect *>(brd->rd);
		kgl_async_upstream *async_us = (kgl_async_upstream *)dr->us;
		async_us->free_ctx(ctx.model_ctx);
	}
}
void KDsoAsyncFetchObject::close(KHttpRequest *rq)
{
	KFetchObject::close(rq);
	if (parser_ctx.header != NULL) {
		parser_ctx.EndParse(rq);
	}
}
KGL_RESULT KDsoAsyncFetchObject::ResponseHeader(KHttpRequest *rq, const char *attr, hlen_t attr_len, const char *val, hlen_t val_len)
{
	if (parser_ctx.ParseHeader(rq, attr, attr_len, val, val_len, false)) {
		return KGL_OK;
	}
	return KGL_EUNKNOW;
}
kev_result KDsoAsyncFetchObject::start_response_body(KHttpRequest *rq)
{
	kassert(rq->ctx->obj);
	parser_ctx.EndParse(rq);
	kassert(rq->ctx->obj->data->headers == NULL);
	rq->ctx->obj->data->headers = parser_ctx.StealHeader();
	return handleUpstreamRecvedHead(rq);
}
kev_result KDsoAsyncFetchObject::open(KHttpRequest *rq)
{
	kassert(brd);
	KDsoRedirect *dr = static_cast<KDsoRedirect *>(brd->rd);
	kgl_async_upstream *async_us = (kgl_async_upstream *)dr->us;
	kgl_memcpy(&ctx, &async_context, sizeof(ctx));
	if (ctx.model_ctx == NULL && async_us->create_ctx) {
		ctx.model_ctx = async_us->create_ctx();
	}
	ctx.cn = rq;	
	parser_ctx.proto = Proto_fcgi;
	return async_us->open(&ctx);
}
kev_result KDsoAsyncFetchObject::readBody(KHttpRequest *rq)
{
	KDsoRedirect *dr = static_cast<KDsoRedirect *>(brd->rd);
	kgl_async_upstream *async_us = (kgl_async_upstream *)dr->us;
	kassert(rq == ctx.cn);
	return async_us->read_body(&ctx);
}
kev_result KDsoAsyncFetchObject::ReadPostCallback(KHttpRequest *rq,int got)
{
	rq->left_read -= got;
	rq->AddUpFlow(got);
	KDsoRedirect *dr = static_cast<KDsoRedirect *>(brd->rd);
	kgl_async_upstream *async_us = (kgl_async_upstream *)dr->us;
	return async_us->read_post_callback(&ctx,got);
}
