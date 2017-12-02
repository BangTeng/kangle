#ifndef KSQLITEDISKCACHEINDEX_H
#define KSQLITEDISKCACHEINDEX_H
#include "KDiskCacheIndex.h"
#ifdef ENABLE_SQLITE_DISK_INDEX
#include "sqlite3.h"
class KSqliteDiskCacheIndex : public KDiskCacheIndex
{
public:
	KSqliteDiskCacheIndex()
	{
		db = NULL;
		fileName = NULL;
	}
	~KSqliteDiskCacheIndex()
	{
		close();
		if (fileName) {
			xfree(fileName);
		}
	}
	void close()
	{
		if (db) {
			sqlite3_close(db);
			db = NULL;
		}
	}
	bool begin();
	bool commit();
	bool create(const char *indexFileName);
	bool open(const char *indexFileName);
	INT64 memory_used();
protected:
	bool add(unsigned filename1,unsigned filename2,const char *url,time_t t,const char *data,int dataLen);
	bool del(unsigned filename1,unsigned filename2);
	bool update(unsigned filename1,unsigned filename2,const char *buf,int len);
	bool updateLast(unsigned filename1,unsigned filename2,time_t t);
	
	bool load(loadDiskCacheIndexCallBack callBack);
private:
	bool check();
	void setting();
	sqlite3 *db;
	char *fileName;
};
#endif
#endif
