#ifndef KDISKCACHEINDEX_H
#define KDISKCACHEINDEX_H
#include "KAsyncWorker.h"
#include "KDiskCache.h"
#ifdef ENABLE_DB_DISK_INDEX
class KHttpObject;
enum ci_operator
{
	ci_add,
	ci_del,
	ci_update,
	ci_updateLast,
	ci_close,
	ci_begin,
	ci_commit,
	ci_load
};
struct diskCacheOperatorParam
{
	ci_operator op;
	unsigned filename1;
	unsigned filename2;	
	HttpObjectIndex data;
	char *url;
};
typedef void (*loadDiskCacheIndexCallBack) (const char *url,const char *data,int dataLen);
class KDiskCacheIndex
{
public:
	KDiskCacheIndex();
	virtual ~KDiskCacheIndex();
	void start(ci_operator op,KHttpObject *obj);
	virtual bool create(const char *indexFileName) = 0;
	virtual bool open(const char *indexFileName) = 0;
	virtual INT64 memory_used()
	{
		return 0;
	}
	void work(diskCacheOperatorParam *param);
	bool allWorkedDone()
	{
		return worker->isEmpty();
	}
	KAsyncWorker *getWorker()
	{
		return worker;
	}
	virtual void close() = 0;
	int load_count;
protected:	
	virtual bool begin() = 0;
	virtual bool commit() = 0;
	virtual bool add(unsigned filename1,unsigned filename2,const char *url,time_t t,const char *data,int dataLen) = 0;
	virtual bool del(unsigned filename1,unsigned filename2) = 0;
	virtual bool update(unsigned filename1,unsigned filename2,const char *data,int dataLen) = 0;
	virtual bool updateLast(unsigned filename1,unsigned filename2,time_t t) = 0;
	virtual bool load(loadDiskCacheIndexCallBack callBack) = 0;
private:
	void insertTranscation();
	KAsyncWorker *worker;
	bool transaction;
};
extern KDiskCacheIndex *dci;
#endif
#endif
