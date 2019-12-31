#ifndef KDISKCACHESTREAM_H
#define KDISKCACHESTREAM_H
#include "global.h"
#include "kmalloc.h"
#include "kasync_file.h"
#ifdef ENABLE_DISK_CACHE
class KHttpObject;
class KHttpRequest;
class KDiskCacheStream;
class KDiskCacheContext {
public:
	KDiskCacheContext()
	{
		memset(this, 0, sizeof(*this));
	}
	~KDiskCacheContext()
	{
		if (buffer) {
			aio_free_buffer(buffer);
		}
	}
	char *buffer;
	char *hot;
	INT64 offset;
	KDiskCacheStream *disk_cache;
	int size;
};
class KDiskCacheStream {
public:
	KDiskCacheStream() {
		memset(this, 0, sizeof(KDiskCacheStream));
	}
	~KDiskCacheStream()
	{
		if (aio_file) {
			kasync_file_close(aio_file);
			kassert(filename);
			unlink(filename);
		}
		if (filename) {
			xfree(filename);
		}
		if (buffer) {
			aio_free_buffer(buffer);
		}
	}
	bool Open(KHttpRequest *rq,KHttpObject *obj);
	bool Write(KHttpRequest *rq, KHttpObject *obj,const char *buf, int len);
	bool Close(KHttpRequest *rq, KHttpObject *obj);
	INT64 GetLength()
	{
		if (buffer == NULL) {
			return offset;
		}
		return offset + (hot - buffer);
	}
	void RealClose();
	kasync_file *GetFile()
	{
		return aio_file;
	}
private:
	void FlushBuffer(KHttpRequest *rq,int head_size);
	char *filename;
	kasync_file *aio_file;
	char *buffer;
	char *hot;
	int buffer_left;
	INT64 offset;
};
#endif
#endif
