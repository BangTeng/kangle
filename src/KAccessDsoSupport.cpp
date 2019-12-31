#include "KHttpRequest.h"
#include "KAccessDsoSupport.h"
#include "http.h"
#include "ssl_utils.h"
#include "KDsoRedirect.h"

#define ADD_VAR(x,y,z) add_api_var(x,y,z,sizeof(z)-1)
KGL_RESULT add_api_var(LPVOID buffer, LPDWORD size, const char *val, int len)
{
	if (len == 0) {
		len = strlen(val);
	}
	if ((int)*size <= len) {
		*size = len + 1;
		return KGL_EINSUFFICIENT_BUFFER;
	}
	kgl_memcpy(buffer, val, len + 1);
	*size = len;
	return KGL_OK;
}
KGL_RESULT var_printf(LPVOID buffer, LPDWORD size, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	int len = vsnprintf((char *)buffer, *size, fmt, ap);
	va_end(ap);
	if (len < (int)*size) {
		*size = len;
		return KGL_OK;
	}
	*size = len + 1;
	return KGL_EINSUFFICIENT_BUFFER;
}
KGL_RESULT add_header_var(LPVOID buffer, LPDWORD size, KHttpHeader *header, const char *name)
{
	while (header) {
		if (attr_casecmp(header->attr, name) == 0) {
			return add_api_var(buffer, size, header->val, 0);
		}
		header = header->next;
	}
	return KGL_ENO_DATA;
}
static void *alloc_memory(KCONN cn, DWORD  cbSize, KF_ALLOC_MEMORY_TYPE memory_type)
{
	KAccessRequest *ar = (KAccessRequest *)cn;
	if (memory_type == KF_ALLOC_REQUEST) {
		return ar->rq->alloc_request_memory(cbSize);
	}
	return ar->rq->alloc_connect_memory(cbSize);
}
KGL_RESULT get_request_variable(KHttpRequest *rq,KGL_VAR type, LPSTR  name, LPVOID  buffer, LPDWORD size)
{
	switch (type) {
	case KGL_VAR_HEADER:
		return add_header_var(buffer, size, rq->GetHeader(), name);
#ifdef KSOCKET_SSL
	case KGL_VAR_SSL_VAR:
	{
		kssl_session *ssl = rq->sink->GetSSL();
		if (ssl) {
			char *result = ssl_var_lookup(ssl->ssl, name);
			if (result) {
				KGL_RESULT ret = add_api_var(buffer, size, result);
				OPENSSL_free(result);
				return ret;
			}
		}
		return KGL_EUNKNOW;
	}
#endif
	case KGL_VAR_HTTPS:
	{
		int *v = (int *)buffer;
		if (TEST(rq->url->flags, KGL_URL_SSL)) {
			*v = 1;
		} else {
			*v = 0;
		}
		return KGL_OK;
	}
	case KGL_VAR_SERVER_PROTOCOL:
		if (rq->http_major > 1) {
			return ADD_VAR(buffer, size, "HTTP/2");
		}
		return ADD_VAR(buffer, size, "HTTP/1.1");
	case KGL_VAR_SERVER_NAME:
		return add_api_var(buffer, size, rq->url->host);
	case KGL_VAR_REQUEST_METHOD:
		return add_api_var(buffer, size, rq->getMethod());
	case KGL_VAR_PATH_INFO:
		return add_api_var(buffer, size, rq->url->path);
	case KGL_VAR_REQUEST_URI:
		if (rq->raw_url.param == NULL) {
			return add_api_var(buffer, size, rq->raw_url.path);
		}
		return var_printf(buffer, size, "%s?%s", rq->raw_url.path, rq->raw_url.param);
	case KGL_VAR_SCRIPT_NAME:
		return add_api_var(buffer, size, rq->url->path);
	case KGL_VAR_QUERY_STRING:
	{
		const char *param = rq->url->param;
		*size = 0;
		KGL_RESULT ret = KGL_ENO_DATA;
		if (param) {
			ret = add_api_var(buffer, size, param);
		}
		return ret;
	}
	case KGL_VAR_SERVER_ADDR:
	{
		sockaddr_i self_addr;
		rq->sink->GetSelfAddr(&self_addr);
		char ips[MAXIPLEN];
		ksocket_sockaddr_ip(&self_addr, ips, sizeof(ips));
		return add_api_var(buffer, size, ips);
	}
	case KGL_VAR_SERVER_PORT:
	{
		uint16_t *v = (uint16_t *)buffer;
		*v = rq->raw_url.port;
		return KGL_OK;
	}
	case KGL_VAR_REMOTE_ADDR:
	{
		sockaddr_i self_addr;
		rq->sink->GetSelfAddr(&self_addr);
		char ips[MAXIPLEN];
		ksocket_sockaddr_ip(&self_addr, ips, sizeof(ips));
		return add_api_var(buffer, size, ips);
	}
	case KGL_VAR_REMOTE_PORT:
	{
		uint16_t *v = (uint16_t *)buffer;
		*v = ksocket_addr_port(rq->sink->GetAddr());
		return KGL_OK;
	}
	case KGL_VAR_PEER_ADDR:
	{
		char ips[MAXIPLEN];
		rq->sink->GetRemoteIp(ips, MAXIPLEN);
		return add_api_var(buffer, size, ips);
	}
	case KGL_VAR_DOCUMENT_ROOT:
		if (!rq->svh) {
			return KGL_ENO_DATA;
		}
		return add_api_var(buffer, size, rq->svh->doc_root);
	case KGL_VAR_CONTENT_LENGTH:
	{
		INT64 *v = (INT64 *)buffer;
		*v = rq->content_length;
		return KGL_OK;
	}
	case KGL_VAR_CONTENT_TYPE:
		return add_header_var(buffer, size, rq->GetHeader(), "Content-Type");
	case KGL_VAR_CONTENT_LEFT:
	{
		INT64 *v = (INT64 *)buffer;
		*v = rq->left_read;
		return KGL_OK;
	}
	case KGL_VAR_IF_MODIFIED_SINCE:
	{
		if (rq->ctx->mt == modified_if_range_date || rq->ctx->lastModified <= 0) {
			return KGL_ENO_DATA;
		}
		time_t *v = (time_t *)buffer;
		*v = rq->ctx->lastModified;
		return KGL_OK;
	}
	case KGL_VAR_IF_RANGE_TIME:
	{
		if (rq->ctx->mt != modified_if_range_date || rq->ctx->lastModified <= 0) {
			return KGL_ENO_DATA;
		}
		time_t *v = (time_t *)buffer;
		*v = rq->ctx->lastModified;
		return KGL_OK;
	}
	case KGL_VAR_IF_NONE_MATCH:
	{
		if (rq->ctx->mt == modified_if_range_etag || rq->ctx->if_none_match == NULL) {
			return KGL_ENO_DATA;
		}
		return add_api_var(buffer, size, rq->ctx->if_none_match->data, rq->ctx->if_none_match->len);
	}
	case KGL_VAR_IF_RANGE_STRING:
	{
		if (rq->ctx->mt != modified_if_range_etag || rq->ctx->if_none_match == NULL) {
			return KGL_ENO_DATA;
		}
		return add_api_var(buffer, size, rq->ctx->if_none_match->data, rq->ctx->if_none_match->len);		
	}
	default:
		return KGL_ENOT_SUPPORT;
	}
	/*
	if (strcasecmp(name, "AUTH_USER") == 0) {
		if (rq->auth) {
			return add_api_var(buffer, size, rq->auth->getUser());
		}
		return KGL_ENO_DATA;
	}
	*/
}
static KGL_RESULT  get_variable(
	KCONN cn,
	KGL_VAR type,
	LPSTR name,
	LPVOID buffer,
	LPDWORD size
) {
	KAccessRequest *ar = (KAccessRequest *)cn;
	return get_request_variable(ar->rq,type, name, buffer, size);
}

static KGL_RESULT  response_header(
	KCONN cn,
	const char *             attr,
	hlen_t attr_len,
	const char *             val,
	hlen_t val_len)
{
	KAccessRequest *ar = (KAccessRequest *)cn;
	if (TEST(ar->rq->flags, RQ_HAS_SEND_HEADER)) {
		return KGL_EHAS_SEND_HEADER;
	}
	if (strcasecmp(attr, "Status") == 0) {
		ar->rq->responseStatus(atoi(val));
		return KGL_OK;
	}
	ar->rq->responseHeader(attr, attr_len>0?attr_len:(hlen_t)strlen(attr), val, val_len>0?val_len:(hlen_t)strlen(val));
	return KGL_OK;
}
static KGL_RESULT  request_write_client (
	KCONN cn,
	LPVOID                        Buffer,
	LPDWORD                       lpdwBytes
)
{
	KAccessRequest *ar = (KAccessRequest *)cn;
	int len = ar->GetBuffer()->write((char *)Buffer, *lpdwBytes);
	if (len <= 0) {
		return KGL_EUNKNOW;
	}
	*lpdwBytes = len;
	return KGL_OK;
}
static KGL_RESULT  response_write_client(
	KCONN cn,
	LPVOID                        Buffer,
	LPDWORD                       lpdwBytes
) {
	return KGL_ENOT_SUPPORT;
}
static KGL_RESULT support_function(
	KCONN                        cn,
	KF_REQ_TYPE                  req,
	PVOID                        data,
	PVOID                        *ret
)
{
	KAccessRequest *ar = (KAccessRequest *)cn;
	switch (req) {
	case KF_REQ_UPSTREAM:
	{
		kgl_upstream *us = (kgl_upstream *)data;
		CLR(us->flags, KF_UPSTREAM_SYNC);
		KDsoRedirect *rd = new KDsoRedirect("", us);
		KFetchObject *fo = rd->makeFetchObject(ar->rq, *ret);
		fo->bindRedirect(rd,KGL_CONFIRM_FILE_NEVER);
		fo->filter = 1;
		ar->rq->pushFetchObject(fo);
		return KGL_OK;
	}
	default:
		return KGL_ENOT_SUPPORT;
	}
}
void init_access_dso_support(kgl_access_context *ctx, int notify)
{
	memset(ctx, 0, sizeof(kgl_access_context));
	ctx->alloc_memory = alloc_memory;
	ctx->support_function = support_function;
	if (TEST(notify, KF_NOTIFY_REQUEST_ACL | KF_NOTIFY_REQUEST_MARK)) {
		ctx->write_client = request_write_client;		
		ctx->response_header = response_header;
		ctx->get_variable = get_variable;
		return;
	}
	ctx->write_client = response_write_client;	
}
