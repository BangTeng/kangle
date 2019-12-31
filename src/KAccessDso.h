#ifndef KACCESSDSO_H
#define KACCESSDSO_H
#include "KMark.h"
#include "KAcl.h"
#include "ksapi.h"
#include "KDsoExtend.h"
#include "KAccessDsoSupport.h"
#include "KBufferFetchObject.h"

#ifdef ENABLE_KSAPI_FILTER
class KAccessDso
{
public:
	KAccessDso(kgl_access *access, KDsoExtend *dso,int notify_type)
	{
		this->access = access;
		this->dso = dso;
		init_access_dso_support(&ctx,notify_type);
		ctx.model_ctx = access->create_ctx();
		this->notify_type = notify_type;
	}
	~KAccessDso()
	{
		if  (ctx.model_ctx)  {
			access->free_ctx(ctx.model_ctx);
		}
	}
	KF_STATUS_TYPE process(KAccessRequest *rq,DWORD notify);
	KAccessDso *newInstance()
	{
		return new KAccessDso(access,dso,notify_type);
	}
	const char *getName()
	{
		return access->name;
	}
	std::string getHtml(KModel *model);
	std::string getDisplay();
	void editHtml(std::map<std::string, std::string> &attribute);
	void buildXML(std::stringstream &s);
	friend class KAccessDsoMark;
	friend class KAccessDsoAcl;
private:
	std::string build(KF_ACCESS_BUILD_TYPE type);
	kgl_access_context ctx;
	kgl_access *access;
	KDsoExtend *dso;
	int notify_type;
};
class KAccessDsoMark : public KMark
{
public:
	KAccessDsoMark(KAccessDso *ad)
	{
		this->ad = ad;
	}
	~KAccessDsoMark()
	{
		delete ad;
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj,const int chainJumpType, int &jumpType)
	{
		KAccessRequest ar(rq);
		switch (ad->process(&ar,obj? KF_NOTIFY_RESPONSE_MARK:KF_NOTIFY_REQUEST_MARK)) {
		case KF_STATUS_REQ_ACCESS_TRUE:
			return true;
		case KF_STATUS_REQ_ACCESS_FALSE:
			return false;
		case KF_STATUS_REQ_FINISHED:
			SET(rq->flags, RQ_CONNECTION_CLOSE);
		default:
			if (ar.buffer) {
				kassert(TEST(ad->notify_type, KF_NOTIFY_RESPONSE_MARK | KF_NOTIFY_RESPONSE_ACL ) == 0);
				rq->closeFetchObject();				
				rq->responseConnection();
				rq->responseHeader(kgl_expand_string("Content-Length"), ar.buffer->getLen());
				rq->fetchObj = new KBufferFetchObject(ar.buffer);
				if (rq->status_code == 0) {
					rq->responseStatus(STATUS_OK);
				}
			}
			jumpType = JUMP_FINISHED;
			return true;
		}
	}
	KMark *newInstance()
	{
		return new KAccessDsoMark(ad->newInstance());
	}
	const char *getName()
	{
		return ad->getName();
	}
	std::string getHtml(KModel *model)
	{
		return ad->getHtml(model);
	}
	std::string getDisplay()
	{
		return ad->getDisplay();
	}
	void editHtml(std::map<std::string, std::string> &attribute)
	{
		ad->editHtml(attribute);
	}
	void buildXML(std::stringstream &s)
	{
		ad->buildXML(s);
	}
private:
	KAccessDso *ad;
};
class KAccessDsoAcl : public KAcl
{
public:
	KAccessDsoAcl(KAccessDso *ad)
	{
		this->ad = ad;
	}
	~KAccessDsoAcl()
	{
		delete ad;
	}
	bool match(KHttpRequest *rq, KHttpObject *obj)
	{
		KAccessRequest ar(rq);
		KF_STATUS_TYPE ret = ad->process(&ar, obj ? KF_NOTIFY_RESPONSE_ACL : KF_NOTIFY_REQUEST_ACL);
		switch (ret) {
		case KF_STATUS_REQ_ACCESS_FALSE:
			return false;
		case KF_STATUS_REQ_ACCESS_TRUE:
			return true;
		default:
			klog(KLOG_ERR, "access dso [%s] process result=[%d] is illegal.\n", this->getName(), ret);
		}
		return false;
	}
	KAcl *newInstance()
	{
		return new KAccessDsoAcl(ad->newInstance());
	}
	const char *getName()
	{
		return ad->getName();
	}
	std::string getHtml(KModel *model)
	{
		return ad->getHtml(model);
	}
	std::string getDisplay()
	{
		return ad->getDisplay();
	}
	void editHtml(std::map<std::string, std::string> &attribute)
	{
		ad->editHtml(attribute);
	}
	void buildXML(std::stringstream &s)
	{
		ad->buildXML(s);
	}
private:
	KAccessDso *ad;
};
#endif
#endif
