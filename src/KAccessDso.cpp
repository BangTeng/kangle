#include "KAccessDso.h"
#include "KXml.h"
#include "ksapi.h"
#include "KHttpRequest.h"
#include "KHttpFilterContext.h"
#include "KHttpFilterHook.h"
#include "KHttpObject.h"
#include "KAccessDsoSupport.h"

#ifdef ENABLE_KSAPI_FILTER
static int write_string(void *serverCtx,const char *str,int len,int build_flags)
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
static const char *get_value(void *server_ctx,const char *name)
{
	std::map<std::string, std::string> *attribute = (std::map<std::string, std::string>*)server_ctx;
	std::map<std::string, std::string>::iterator it = attribute->find(name);
	if (it==attribute->end()) {
		return NULL;
	}
	return (*it).second.c_str();
}
KF_STATUS_TYPE KAccessDso::process(KAccessRequest *ar,DWORD notify)
{
	kgl_access_context ar_ctx;
	memcpy(&ar_ctx, &ctx, sizeof(ar_ctx));
	ar_ctx.cn = ar;
	return access->process(&ar_ctx,notify);
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
	parser.model_ctx = ctx.model_ctx;
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
	builder.model_ctx = ctx.model_ctx;
	builder.write_string = write_string;
	access->build(&builder,type);
	return s.str();
}
#endif
