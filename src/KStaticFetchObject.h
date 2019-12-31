#ifndef KSTATICFETCHOBJECT_H
#define KSTATICFETCHOBJECT_H
#include <stdlib.h>
#include "global.h"
#include "KFetchObject.h"
#include "kasync_file.h"
#include "kfile.h"
class KAsyncData
{
public:
	KAsyncData(kasync_file *fp,int buf_size)
	{
		memset(this,0,sizeof(KAsyncData));
		this->aio_fp = fp;
		buf = (char *)aio_alloc_buffer(buf_size);
		this->buf_size = buf_size;
	}
	~KAsyncData()
	{
		if (aio_fp) {
			kasync_file_close(aio_fp);
		}
		if (buf) {
			aio_free_buffer(buf);
		}
	}
	INT64 offset;
	int buf_size;
	char *buf;
	kasync_file *aio_fp;
};
class KStaticFetchObject : public KFetchObject 
{
public:	
	KStaticFetchObject()
	{
		ad = NULL;
		kfinit(fp);
	}
	~KStaticFetchObject()
	{
		if (ad) {
			delete ad;
		}
		if (kflike(fp)) {
			::kfclose(fp);
		}
	}
	bool NeedTempFile(bool upload, KHttpRequest *rq)
	{
		return false;
	}
	kev_result open(KHttpRequest *rq);
	kev_result readBody(KHttpRequest *rq);
	kev_result handleAsyncReadBody(KHttpRequest *rq,char *buf,int got);
	kev_result asyncReadBody(KHttpRequest *rq);
	kev_result syncReadBody(KHttpRequest *rq);
	void getAsyncBuffer(KHttpRequest *rq,iovec *buf,int &bufCount)
	{
		assert(ad);
		buf[0].iov_base = ad->buf;
		buf[0].iov_len = (int) MIN(rq->file->fileSize,(INT64)sizeof(ad->buf));
	}
private:
	FILE_HANDLE fp;
	KAsyncData *ad;
};
#endif
