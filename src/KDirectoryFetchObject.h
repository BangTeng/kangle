#ifndef KDIRECTORYFETCHOBJECT_H
#define KDIRECTORYFETCHOBJECT_H
#include "KFetchObject.h"
#include "kforwin32.h"
#include "KBuffer.h"
#ifndef _WIN32
#include <sys/types.h>
#include <dirent.h>
#endif
class KPrevDirectoryFetchObject : public KFetchObject
{
public:
	kev_result open(KHttpRequest *rq);
	
	kev_result readBody(KHttpRequest *rq)
	{
		assert(false);
		return kev_err;
	}
};
class KDirectoryFetchObject : public KFetchObject
{
public:
	KDirectoryFetchObject();
	~KDirectoryFetchObject();
	kev_result open(KHttpRequest *rq);
	kev_result readBody(KHttpRequest *rq);
private:
	void browerOneFile(KHttpRequest *rq,const char *path);
#ifdef _WIN32
	HANDLE dp;
	WIN32_FIND_DATA FileData;
#else
	DIR *dp;
#endif
	KBuffer buffer;
	kbuf *hot;
};
#endif
