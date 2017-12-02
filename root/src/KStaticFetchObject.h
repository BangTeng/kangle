#ifndef KSTATICFETCHOBJECT_H
#define KSTATICFETCHOBJECT_H
#include <stdlib.h>
#include "global.h"
#include "KFetchObject.h"
#include "KAsyncSelectable.h"
#include "KFile.h"
class KAsyncData
{
public:
	KAsyncData(KFile *fp)
	{
		memset(this,0,sizeof(KAsyncData));
		as = new KAsyncSelectable(fp);
	}
	~KAsyncData()
	{
		if (as) {
			delete as;
		}
	}
	char buf[8192];
	KAsyncSelectable *as;
};
class KStaticFetchObject : public KFetchObject 
{
public:	
	KStaticFetchObject()
	{
		ad = NULL;
	}
	~KStaticFetchObject()
	{
		if (ad) {
			delete ad;
		}
	}
	bool needTempFile()
	{
		return false;
	}
	void open(KHttpRequest *rq);
	void readBody(KHttpRequest *rq);
	void handleAsyncReadBody(KHttpRequest *rq,int got);	
	void asyncReadBody(KHttpRequest *rq);
	void syncReadBody(KHttpRequest *rq);
	void getAsyncBuffer(KHttpRequest *rq,iovec *buf,int &bufCount)
	{
		assert(ad);
		buf[0].iov_base = ad->buf;
		buf[0].iov_len = (int) MIN(rq->file->fileSize,(INT64)sizeof(ad->buf));
	}
private:
	KFile fp;
	KAsyncData *ad;
	
};
#endif
