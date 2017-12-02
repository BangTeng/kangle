#ifndef KSTUBSTATUSMARK_H_
#define KSTUBSTATUSMARK_H_

#include "KMark.h"

#ifdef ENABLE_STAT_STUB
class KStubStatusMark: public KMark {
public:
	KStubStatusMark()
	{
	}
	~KStubStatusMark()
	{
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj, const int chainJumpType,int &jumpType)
	{
		rq->responseStatus(STATUS_OK);
		rq->responseHeader(kgl_expand_string("Content-Type"), kgl_expand_string("text/plain"));
		rq->responseHeader(kgl_expand_string("Cache-Control"), kgl_expand_string("no-cache,no-store"));
		rq->responseHeader(kgl_expand_string("Server"),conf.serverName,conf.serverNameLength);
		rq->buffer << "Active connections: " << (int)total_connect << " \n";
		rq->buffer << "server accepts handled requests\n";
		rq->buffer << " " <<(INT64)katom_get64((void *)&kgl_total_servers) << " " << (INT64)katom_get64((void *)&kgl_total_accepts) << " " << (INT64)katom_get64((void *)&kgl_total_requests) << " \n";
		rq->buffer << "Reading: " << (int)katom_get((void *)&kgl_reading) << " Writing: " << (int)katom_get((void *)&kgl_writing) << " Waiting: " << (int)katom_get((void *)&kgl_waiting) << " \n";
		rq->responseHeader(kgl_expand_string("Content-Length"),rq->buffer.getLen());
		
		if (TEST(rq->flags,RQ_CONNECTION_CLOSE) || !TEST(rq->flags,RQ_HAS_KEEP_CONNECTION)) {
			rq->responseHeader(kgl_expand_string("Connection"),kgl_expand_string("close"));
		} else {
			rq->responseHeader(kgl_expand_string("Connection"),kgl_expand_string("keep-alive"));
		}
		
		jumpType = JUMP_DENY;
		return true;
	}
	KMark *newInstance()
	{
		return new KStubStatusMark();
	}
	const char *getName()
	{
		return "stub_status";
	}
	std::string getHtml(KModel *model)
	{
		return "";
	}
	std::string getDisplay()
	{
		return "";
	}
	void editHtml(std::map<std::string, std::string> &attribute)
			throw (KHtmlSupportException)
	{
	}
	void buildXML(std::stringstream &s)
	{
		s << ">";
	}
private:
};
#endif
#endif
