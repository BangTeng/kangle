#ifndef KDIRECTORYFETCHOBJECT_H
#define KDIRECTORYFETCHOBJECT_H
#include "KFetchObject.h"
#include "forwin32.h"
#include "KBuffer.h"
#ifndef _WIN32
#include <sys/types.h>
#include <dirent.h>
#endif
class KPrevDirectoryFetchObject : public KFetchObject
{
public:
	void open(KHttpRequest *rq);
	
	void readBody(KHttpRequest *rq)
	{
		assert(false);
	}
};
class KDirectoryFetchObject : public KFetchObject
{
public:
	KDirectoryFetchObject();
	~KDirectoryFetchObject();
	void open(KHttpRequest *rq);
	void readBody(KHttpRequest *rq);
private:
	void browerOneFile(KHttpRequest *rq,const char *path);
#ifdef _WIN32
	HANDLE dp;
	WIN32_FIND_DATA FileData;
#else
	DIR *dp;
#endif
	KBuffer buffer;
	buff *hot;
};
#endif
