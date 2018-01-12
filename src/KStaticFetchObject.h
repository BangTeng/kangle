#ifndef KSTATICFETCHOBJECT_H
#define KSTATICFETCHOBJECT_H
#include <stdlib.h>
#include "global.h"
#include "KFetchObject.h"
#include "KAsyncFile.h"
#include "KFile.h"
class KAsyncData
{
public:
	KAsyncData(KAsyncFile *fp,int buf_size)
	{
		memset(this,0,sizeof(KAsyncData));
		this->aio_fp = fp;
		buf = (char *)aio_alloc_buffer(buf_size);
		this->buf_size = buf_size;
	}
	~KAsyncData()
	{
		if (aio_fp) {
			delete aio_fp;
		}
		if (buf) {
			aio_free_buffer(buf);
		}
	}
	INT64 offset;
	int buf_size;
	char *buf;
	KAsyncFile *aio_fp;
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
	void handleAsyncReadBody(KHttpRequest *rq,char *buf,int got);	
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
