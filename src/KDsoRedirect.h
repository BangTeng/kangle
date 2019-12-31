#ifndef KDSOREDIRECT_H
#define KDSOREDIRECT_H
#include "KRedirect.h"
#include "ksapi.h"


class KDsoRedirect : public KRedirect
{
public:
	KDsoRedirect(const char *dso_name,kgl_upstream *us)
	{
		name = dso_name;
		name += ":";
		name += us->name;
		this->us = us;
	}
	~KDsoRedirect()
	{
	
	}
	const char *getType() {
		return "dso";
	}
	KFetchObject *makeFetchObject(KHttpRequest *rq, KFileName *file);
	KFetchObject *makeFetchObject(KHttpRequest *rq, void *model_ctx);
	friend class KDsoExtend;
	friend class KDsoAsyncFetchObject;
private:
	kgl_upstream *us;
};
#endif
