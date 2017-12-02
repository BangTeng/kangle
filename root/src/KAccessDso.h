#ifndef KACCESSDSO_H
#define KACCESSDSO_H
#include "KMark.h"
#include "KAcl.h"
#include "ksapi.h"
#include "KHttpFilterDso.h"
#ifdef ENABLE_KSAPI_FILTER
class KAccessDso
{
public:
	KAccessDso(kgl_access *access,KHttpFilterDso *dso,int notify_type)
	{
		this->access = access;
		this->dso = dso;
		ctx = access->create_ctx();
		this->notify_type = notify_type;
	}
	~KAccessDso()
	{
		if  (ctx)  {
			access->free_ctx(ctx);
		}
	}
	DWORD process(KHttpRequest *rq, KHttpObject *obj);
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
	void editHtml(std::map<std::string, std::string> &attribute)
			throw (KHtmlSupportException);
	void buildXML(std::stringstream &s);
private:
	std::string build(KF_ACCESS_BUILD_TYPE type);
	void *ctx;
	kgl_access *access;
	KHttpFilterDso *dso;
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
		switch (ad->process(rq,obj)) {
		case KF_STATUS_REQ_ACCESS_TRUE:
			return true;
		case KF_STATUS_REQ_ACCESS_FALSE:
			return false;
		case KF_STATUS_REQ_FINISHED:
			SET(rq->flags, RQ_CONNECTION_CLOSE);
		default:
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
			throw (KHtmlSupportException)
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
		if (KF_STATUS_REQ_ACCESS_FALSE==ad->process(rq,obj)) {
			return false;
		}
		return true;
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
			throw (KHtmlSupportException)
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
