#ifndef KCACHESTREAM_H
#define KCACHESTREAM_H
#include "KHttpStream.h"
#include "KHttpObject.h"
#include "KDiskCacheStream.h"

enum cache_model
{
	cache_none,
	cache_memory,
	cache_disk
};

inline void set_buffer_obj(KAutoBuffer *buffer,KHttpObject *obj)
{
	assert(obj->data->bodys==NULL);
	obj->index.content_length = buffer->getLen();
	obj->data->bodys = buffer->stealBuff();
	SET(obj->index.flags,OBJ_IS_READY);
}
class KCacheStream : public KHttpStream
{
public:
	KCacheStream(KWStream *st,bool autoDelete);
	~KCacheStream();
	void init(KHttpRequest *rq, KHttpObject *obj, cache_model cache_layer);
	StreamState write_direct(char *buf,int len);
	StreamState write_all(const char *buf,int len);
	StreamState write_end();
private:
	void CheckMemoryCacheSize();
#ifdef ENABLE_DISK_CACHE	
	KDiskCacheStream *NewDiskCache();
	KDiskCacheStream *disk_cache;
#endif
	KHttpRequest *rq;
	KHttpObject *obj;
	KAutoBuffer *buffer;
};
#endif
