#include "KDsoAsyncFetchObject.h"
#include "KDsoRedirect.h"
#include "http.h"
static KGL_RESULT push_data(KCONN    cn,LPVOID   buf,	DWORD   len)
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
static kev_result try_write(KCONN  cn)
{
	return try_send_request((KHttpRequest *)cn);
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
static KGL_RESULT response_header(KCONN cn,		const char *attr,	hlen_t attr_len,	const char *val,	hlen_t val_len) 
{
	KHttpRequest *rq = (KHttpRequest *)cn;
	KDsoAsyncFetchObject *fo = static_cast<KDsoAsyncFetchObject *>(rq->fetchObj);
	return fo->response_header(rq, attr, attr_len, val, val_len);
}
KDsoAsyncFetchObject::~KDsoAsyncFetchObject()
{
	if (ctx.model_ctx) {
		kassert(brd);
		KDsoRedirect *dr = static_cast<KDsoRedirect *>(brd->rd);
		kgl_async_upstream *async_us = (kgl_async_upstream *)dr->us;
		async_us->free_ctx(ctx.model_ctx);
	}
}
KGL_RESULT KDsoAsyncFetchObject::response_header(KHttpRequest *rq, const char *attr, hlen_t attr_len, const char *val, hlen_t val_len)
{
#if 0
	if (parser_ctx.responseTime == 0) {
		parser_ctx.StartParse(rq);
	}
#endif
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
	if (ctx.model_ctx == NULL && async_us->create_ctx) {
		ctx.model_ctx = async_us->create_ctx();
	}
	ctx.content_length = rq->content_length;
	ctx.cn = rq;
	ctx.left = &rq->left_read;
	ctx.response_header = ::response_header;
	ctx.start_response_body = ::start_response_body;
	ctx.end_request = end_request;
	ctx.try_write = try_write;
	ctx.push_data = push_data;
	parser_ctx.proto = Proto_fcgi;
	return async_us->open(&ctx);
}
kev_result KDsoAsyncFetchObject::readBody(KHttpRequest *rq)
{
	KDsoRedirect *dr = static_cast<KDsoRedirect *>(brd->rd);
	kgl_async_upstream *async_us = (kgl_async_upstream *)dr->us;
	return async_us->read_body(&ctx);
}
