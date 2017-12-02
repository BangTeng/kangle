#include "KAccessDso.h"
#include "KXml.h"
#include "ksapi.h"
#include "KHttpRequest.h"
#include "KHttpFilterContext.h"
#include "KHttpFilterHook.h"
#include "KHttpObject.h"
#ifdef ENABLE_KSAPI_FILTER
static int WINAPI  write_string(void *serverCtx,const char *str,int len,int build_flags)
{
	std::stringstream *s = (std::stringstream *)serverCtx;
	if (TEST(build_flags, KGL_BUILD_HTML_ENCODE)) {
		char *buf = KXml::htmlEncode(str, len, NULL);
		s->write(buf, len);
		free(buf);
		return 0;
	}
	s->write(str, len);
	return 0;
}
static const char *WINAPI  get_value(void *server_ctx,const char *name)
{
	std::map<std::string, std::string> *attribute = (std::map<std::string, std::string>*)server_ctx;
	std::map<std::string, std::string>::iterator it = attribute->find(name);
	if (it==attribute->end()) {
		return NULL;
	}
	return (*it).second.c_str();
}
DWORD KAccessDso::process(KHttpRequest *rq, KHttpObject *obj)
{
	rq->init_http_filter();
	rq->http_filter_ctx->restore(dso->index,ctx);
	DWORD ret;
	if (TEST(notify_type,KF_NOTIFY_REQUEST_ACL|KF_NOTIFY_REQUEST_MARK)) {
		kgl_filter_request notification;
		init_kgl_filter_request(notification);
		ret = access->process(&rq->http_filter_ctx->ctx,
				notify_type,
				&notification);

	} else {
		kgl_filter_response notification;
		init_kgl_filter_response(notification);
		notification.HttpStatus = obj->data->status_code;
		ret = access->process(&rq->http_filter_ctx->ctx,
				notify_type,
				&notification);
		obj->data->status_code = (unsigned short)notification.HttpStatus;
	}
	rq->http_filter_ctx->save(dso->index);
	return ret;
}
std::string KAccessDso::getHtml(KModel *model)
{
	return build(KF_ACCESS_BUILD_HTML);
}
std::string KAccessDso::getDisplay()
{
	return build(KF_ACCESS_BUILD_SHORT);
}
void KAccessDso::editHtml(std::map<std::string, std::string> &attribute)
		throw (KHtmlSupportException)
{
	kgl_access_parse parser;
	memset(&parser,0,sizeof(parser));
	parser.get_value = get_value;
	parser.model_ctx = ctx;
	parser.server_ctx = &attribute;
	access->parse(&parser);
}
void KAccessDso::buildXML(std::stringstream &s)
{
	s << build(KF_ACCESS_BUILD_XML)  << ">";
}
std::string KAccessDso::build(KF_ACCESS_BUILD_TYPE type)
{
	std::stringstream s;
	kgl_access_build builder;
	memset(&builder,0,sizeof(builder));
	builder.server_ctx = &s;
	builder.model_ctx = ctx;
	builder.write_string = write_string;
	access->build(&builder,type);
	return s.str();
}
#endif
