#include "forwin32.h"
#include "KHttpFilterContext.h"
#include "KHttpRequest.h"
#include "KHttpFilterDsoManage.h"
#include "KSubVirtualHost.h"
#include "KSelectorManager.h"

#include "http.h"
#include "ssl_utils.h"
#ifdef ENABLE_KSAPI_FILTER
#define ADD_VAR(x,y,z) add_api_var(x,y,z,sizeof(z)-1)
static void * WINAPI AllocMem(kgl_filter_context * pfc,DWORD  cbSize, KF_ALLOC_MEMORY_TYPE memory_type)
{
	KHttpRequest *rq = (KHttpRequest *)pfc->ServerContext;
	assert(rq->http_filter_ctx);
	if (memory_type == KF_ALLOC_REQUEST) {
		return rq->alloc_request_memory(cbSize);
	}
	return rq->alloc_connect_memory(cbSize);
}
KGL_RESULT add_api_var(LPVOID buffer, LPDWORD size, const char *val, int len)
{
	if (len == 0) {
		len = strlen(val);
	}
	if ((int)*size <= len) {
		*size = len + 1;
		return KGL_EINSUFFICIENT_BUFFER;
	}
	memcpy(buffer,val,len+1);
	*size = len;
	return KGL_OK;
}
KGL_RESULT var_printf(LPVOID buffer,LPDWORD size,const char *fmt,...) {
	va_list ap;
	va_start(ap,fmt);
	int len = vsnprintf((char *)buffer,*size,fmt,ap);
	va_end(ap);
	if (len < (int)*size) {
		*size = len;
		return KGL_OK;
	}
	*size = len + 1;
	return KGL_EINSUFFICIENT_BUFFER;
}
KGL_RESULT add_header_var(LPVOID buffer,LPDWORD size,KHttpHeader *header,const char *name)
{
	while (header) {
		if (attr_casecmp(header->attr,name) == 0) {
			return add_api_var(buffer,size,header->val,0);
		}
		header = header->next;
	}
	return KGL_ENO_DATA;
}
static KGL_RESULT WINAPI  GetServerVariable (
	kgl_filter_context * pfc,
	LPSTR                         name,
	LPVOID                        buffer,
	LPDWORD                       size
	)
{
	KHttpRequest *rq = (KHttpRequest *)pfc->ServerContext;
	if (strcasecmp(name, "SERVER_PROTOCOL")==0) {
#ifdef ENABLE_HTTP2
		if (rq->http2_ctx) {
			return ADD_VAR(buffer,size,"HTTP/2");
		}
#endif
		return ADD_VAR(buffer,size,"HTTP/1.1");
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
	if (strcasecmp(name,"PATH_INFO")==0) {
		return add_api_var(buffer, size, rq->url->path);
	}
	if (strcasecmp(name, "REQUEST_URI") == 0) {
		if (rq->raw_url.param == NULL) {
			return add_api_var(buffer, size, rq->raw_url.path);
		} 
		return var_printf(buffer,size,"%s?%s",rq->raw_url.path,rq->raw_url.param);
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
		rq->c->socket->get_self_addr(&self_addr);
		char ips[MAXIPLEN];
		KSocket::make_ip(&self_addr,ips,sizeof(ips));
		return add_api_var(buffer, size, ips);
	}
	if (strcasecmp(name, "SERVER_PORT") == 0) {
		return var_printf(buffer,size,"%d",rq->raw_url.port);
	}
	if (strcasecmp(name, "REMOTE_ADDR") == 0) {
		return add_api_var(buffer, size, rq->getClientIp());
	}	
	if (strcasecmp(name, "REMOTE_PORT") == 0) {
		return var_printf(buffer,size,"%d",rq->c->socket->get_remote_port());
	}
	if (strcasecmp(name, "PEER_ADDR") == 0) {
		char ips[MAXIPLEN];
		rq->c->socket->get_remote_ip(ips, MAXIPLEN);
		return add_api_var(buffer, size,ips);
	}
	if (strcasecmp(name, "DOCUMENT_ROOT") == 0) {
		if (!rq->svh) {
			return KGL_ENO_DATA;
		}
		return add_api_var(buffer, size, rq->svh->doc_root);
	}
	if (strcasecmp(name, "CONTENT_LENGTH") == 0) {
		return var_printf(buffer,size,INT64_FORMAT,rq->content_length);
	}
	if (strcasecmp(name, "CONTENT_TYPE") == 0) {
		return add_header_var(buffer,size,rq->parser.headers,"Content-Type");
	}
	if (strcasecmp(name, "HTTPS") == 0) {
		if (TEST(rq->workModel, WORK_MODEL_SSL)) {
			return ADD_VAR(buffer, size, "ON");
		} else {
			return ADD_VAR(buffer, size, "OFF");
		}
	}
	if (strncasecmp(name,"HTTP_",5) == 0) {
		return add_header_var(buffer,size,rq->parser.headers,name+5);
	}
#ifdef KSOCKET_SSL
	if (TEST(rq->workModel, WORK_MODEL_SSL)) {
		KSSLSocket *sslSocket = static_cast<KSSLSocket *>(rq->c->getSocket());
		char *result = ssl_var_lookup(sslSocket->getSSL(),name);
		if (result) {
			KGL_RESULT ret = add_api_var(buffer, size, result);
			OPENSSL_free(result);
			return ret;
		}
	}
#endif
	return KGL_EUNKNOW;
}

static KGL_RESULT WINAPI  AddResponseHeaders (
	kgl_filter_context * pfc,
	LPSTR                attr,
	LPSTR                val,
	DWORD                dwReserved
	)
{
	KHttpRequest *rq = (KHttpRequest *)pfc->ServerContext;
	if (TEST(rq->flags,RQ_HAS_SEND_HEADER)) {
		return KGL_EHAS_SEND_HEADER;
	}
	if (strcasecmp(attr,"Status")==0) {
		rq->responseStatus(atoi(val));
		return KGL_OK;
	}
	rq->responseHeader(attr,(hlen_t)strlen(attr),val, (hlen_t)strlen(val));
	return KGL_OK;
}
static KGL_RESULT WINAPI  WriteClient  (
	kgl_filter_context * pfc,
	LPVOID                        Buffer,
	LPDWORD                       lpdwBytes,
	DWORD                         dwReserved
	)
{
	KHttpRequest *rq = (KHttpRequest *)pfc->ServerContext;
	if (rq->buffer.getLen()>1048576) {
		return KGL_EUNKNOW;
	}
	rq->buffer.write_all((char *)Buffer,*lpdwBytes);
	return KGL_OK;
}
static KGL_RESULT WINAPI  ServerSupportFunction (
	kgl_filter_context * pfc,
	enum KF_REQ_TYPE     kfReq,
	PVOID                pData,
	DWORD                ul1,
	DWORD                ul2
	)
{
	assert(pfc->ulReserved>=0 && (int)pfc->ulReserved < KHttpFilterDsoManage::cur_dso_index);
	KHttpRequest *rq = (KHttpRequest *)pfc->ServerContext;
	assert(rq->http_filter_ctx);
	switch (kfReq) {
	case KF_REQ_DISABLE_NOTIFICATIONS:
		{
			rq->http_filter_ctx->filterContext[pfc->ulReserved].disabled_flags |= ul1;
			return KGL_OK;
		}
	case KF_REQ_CONNECT_CLEAN:
		{
			kgl_call_back *c = (kgl_call_back *)pData;
			rq->registerConnectCleanHook(c->call_back,c->arg);
			return KGL_OK;
		}
	case KF_REQ_REQUEST_CLEAN:
		{
			kgl_call_back *c = (kgl_call_back *)pData;
			rq->registerRequestCleanHook(c->call_back,c->arg);
			return KGL_OK;
		}
	case KF_REQ_REWRITE_URL:
		{
			const char *url = (const char *)pData;
			if (rq->rewriteUrl(url)) {
				return KGL_OK;
			}
			return KGL_EINVALID_PARAMETER;
		}
		/*
	case KF_REQ_SAVE_POST: {
			const char *ctx = (const char *)pData;
			if (ctx == NULL) {
				return KGL_EINVALID_PARAMETER;
			}
			INT64 left_read = rq->left_read - rq->parser.bodyLen;
			if (left_read>0) {
				if (rq->content_length>conf.max_post_size) {
					return KGL_EINSUFFICIENT_BUFFER;
				}
				if (rq->tf) {
					return KGL_EUNKNOW;
				}
				rq->tf = new KTempFile;
				rq->tf->init(left_read);
				int size = strlen(ctx);
				char *arg = (char *)rq->alloc_request_memory(size + 1);
				memcpy(arg, ctx, size + 1);
				if (rq->tf->post_ctx == NULL) {
					rq->tf->post_ctx = new KTempFilePostContext;
				}
				rq->tf->post_ctx->arg = arg;
				//rq->tf->startPost(rq, AfterPostForAttack, arg);
				return KGL_OK;
			}
			return KGL_EUNKNOW;
		}
		
	*/
	default:
		break;
	}
	return KGL_EUNKNOW;
}
KHttpFilterContext::KHttpFilterContext()
{
	memset(&ctx,0,sizeof(ctx));
	filterContext = NULL;
}
KHttpFilterContext::~KHttpFilterContext()
{
	if (filterContext) {
		xfree(filterContext);
	}
}
//static kgl_filter_context global_http_filter_context;
void KHttpFilterContext::init(KHttpRequest *rq)
{
	ctx.ServerContext = (void *)rq;
	ctx.cbSize = sizeof(ctx);
	ctx.alloc_memory = AllocMem;
	ctx.add_headers = AddResponseHeaders;
	ctx.get_variable = GetServerVariable;
	ctx.support_function = ServerSupportFunction;
	ctx.write_client = WriteClient;
	//if (TEST(rq->workModel,WORK_MODEL_SSL)) {
	//	ctx.fIsSecurePort = TRUE;
	//}
	assert(filterContext==NULL);
	assert(KHttpFilterDsoManage::cur_dso_index>0);
	int filterContextSize = sizeof(KHttpFilterRequestContext)*KHttpFilterDsoManage::cur_dso_index;
	filterContext = (KHttpFilterRequestContext *)xmalloc(filterContextSize);
	memset(filterContext,0,filterContextSize);
}
#endif
