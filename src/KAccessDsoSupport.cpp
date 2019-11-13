#include "KHttpRequest.h"
#include "KAccessDsoSupport.h"
#include "http.h"
#include "ssl_utils.h"


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
	memcpy(buffer, val, len + 1);
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
static KGL_RESULT  get_variable(
	KCONN cn,
	LPSTR                         name,
	LPVOID                        buffer,
	LPDWORD                       size
) {
	KAccessRequest *ar = (KAccessRequest *)cn;
	KHttpRequest *rq = ar->rq;
	if (strcasecmp(name, "SERVER_PROTOCOL") == 0) {
		if (rq->http_major > 1) {
			return ADD_VAR(buffer, size, "HTTP/2");
		}
		return ADD_VAR(buffer, size, "HTTP/1.1");
	}
	if (strcasecmp(name, "SERVER_NAME") == 0) {
		return add_api_var(buffer, size, rq->url->host);
	}
	if (strcasecmp(name, "REQUEST_METHOD") == 0) {
		return add_api_var(buffer, size, rq->getMethod());
	}
	if (strcasecmp(name, "AUTH_USER") == 0) {
		if (rq->auth) {
			return add_api_var(buffer, size, rq->auth->getUser());
		}
		return KGL_ENO_DATA;
	}
	if (strcasecmp(name, "PATH_INFO") == 0) {
		return add_api_var(buffer, size, rq->url->path);
	}
	if (strcasecmp(name, "REQUEST_URI") == 0) {
		if (rq->raw_url.param == NULL) {
			return add_api_var(buffer, size, rq->raw_url.path);
		}
		return var_printf(buffer, size, "%s?%s", rq->raw_url.path, rq->raw_url.param);
	}
	if (strcasecmp(name, "SCRIPT_NAME") == 0) {
		return add_api_var(buffer, size, rq->url->path);
	}
	if (strcasecmp(name, "QUERY_STRING") == 0) {
		char *param_buf = NULL;
		const char *param = rq->url->getParam(&param_buf);
		*size = 0;
		KGL_RESULT ret = KGL_ENO_DATA;
		if (param) {
			ret = add_api_var(buffer, size, param);
		}
		if (param_buf) {
			free(param_buf);
		}
		return ret;
	}
	if (strcasecmp(name, "SERVER_ADDR") == 0) {
		sockaddr_i self_addr;
		rq->sink->GetSelfAddr(&self_addr);
		char ips[MAXIPLEN];
		ksocket_sockaddr_ip(&self_addr, ips, sizeof(ips));
		return add_api_var(buffer, size, ips);
	}
	if (strcasecmp(name, "SERVER_PORT") == 0) {
		return var_printf(buffer, size, "%d", rq->raw_url.port);
	}
	if (strcasecmp(name, "REMOTE_ADDR") == 0) {
		return add_api_var(buffer, size, rq->getClientIp());
	}
	if (strcasecmp(name, "REMOTE_PORT") == 0) {
		return var_printf(buffer, size, "%d", ksocket_addr_port(rq->sink->GetAddr()));
	}
	if (strcasecmp(name, "PEER_ADDR") == 0) {
		char ips[MAXIPLEN];
		rq->sink->GetRemoteIp(ips, MAXIPLEN);
		return add_api_var(buffer, size, ips);
	}
	if (strcasecmp(name, "DOCUMENT_ROOT") == 0) {
		if (!rq->svh) {
			return KGL_ENO_DATA;
		}
		return add_api_var(buffer, size, rq->svh->doc_root);
	}
	if (strcasecmp(name, "CONTENT_LENGTH") == 0) {
		return var_printf(buffer, size, INT64_FORMAT, rq->content_length);
	}
	if (strcasecmp(name, "CONTENT_TYPE") == 0) {
		return add_header_var(buffer, size, rq->GetHeader(), "Content-Type");
	}
	if (strcasecmp(name, "HTTPS") == 0) {
		if (TEST(rq->url->flags, KGL_URL_SSL)) {
			return ADD_VAR(buffer, size, "ON");
		} else {
			return ADD_VAR(buffer, size, "OFF");
		}
	}
	if (strncasecmp(name, "HTTP_", 5) == 0) {
		return add_header_var(buffer, size, rq->GetHeader(), name + 5);
	}
#ifdef KSOCKET_SSL
	SSL *ssl = rq->sink->GetSSL();
	if (ssl) {
		char *result = ssl_var_lookup(ssl, name);
		if (result) {
			KGL_RESULT ret = add_api_var(buffer, size, result);
			OPENSSL_free(result);
			return ret;
		}
	}
#endif
	return KGL_EUNKNOW;
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
void init_access_dso_suuport(kgl_access_context *ctx, int notify)
{
	memset(ctx, 0, sizeof(kgl_access_context));
	ctx->alloc_memory = alloc_memory;
	if (TEST(notify, KF_NOTIFY_REQUEST_ACL | KF_NOTIFY_REQUEST_MARK)) {
		ctx->write_client = request_write_client;		
		ctx->response_header = response_header;
		ctx->get_variable = get_variable;
		return;
	}
	ctx->write_client = response_write_client;
}
