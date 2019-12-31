#include<vector>
#include "KCacheStream.h"
#include "do_config.h"
#include "KCache.h"
KCacheStream::~KCacheStream()
{
	if (buffer) {
		delete buffer;
	}
#ifdef ENABLE_DISK_CACHE
	if (disk_cache) {
		delete disk_cache;
	}
#endif
}
void KCacheStream::init(KHttpRequest *rq, KHttpObject *obj, cache_model cache_layer)
{
	this->rq = rq;
	this->obj = obj;
#ifdef ENABLE_DISK_CACHE
	if (cache_layer != cache_memory) {
		buffer = NULL;
		disk_cache = NewDiskCache();
		return;
	}
	disk_cache = NULL;
#endif
	buffer = new KAutoBuffer();
}
KCacheStream::KCacheStream(KWStream *st,bool autoDelete) : KHttpStream(st,autoDelete)
{
	
}
StreamState KCacheStream::write_end()
{
	bool have_cache = (buffer != NULL);
#ifdef ENABLE_DISK_CACHE
	if (disk_cache) {
		have_cache = true;
	}
#endif
	if (have_cache && obj->data->status_code == STATUS_CONTENT_PARTIAL) {
		kassert(obj->IsContentRangeComplete(rq));
		obj->data->status_code = STATUS_OK;
		obj->removeHttpHeader("Content-Range");
		CLR(obj->index.flags, ANSW_HAS_CONTENT_RANGE);
	}
	if (buffer) {
		set_buffer_obj(buffer,obj);
		kassert(disk_cache == NULL);
	}
#ifdef ENABLE_DISK_CACHE
	if (disk_cache) {
		kassert(buffer == NULL);
		disk_cache->Close(rq,obj);
		obj->index.content_length = disk_cache->GetLength();
		kassert(obj->data->bodys == NULL);
		SET(obj->index.flags, OBJ_IS_READY|FLAG_IN_DISK);
		obj->data->type = BIG_OBJECT;
	}
#endif
	return KHttpStream::write_end();
}
void KCacheStream::CheckMemoryCacheSize()
{
	if (buffer->getLen() > conf.max_cache_size) {
#ifdef ENABLE_DISK_CACHE
		if (obj_can_disk_cache(rq, obj)) {
			//turn on disk cache
			kassert(disk_cache == NULL);
			disk_cache = NewDiskCache();
			if (disk_cache) {
				kbuf *buf = buffer->getHead();
				while (buf) {
					if (buf->used > 0 && !disk_cache->Write(rq, obj, buf->data, buf->used)) {
						delete disk_cache;
						disk_cache = NULL;
						break;
					}
					buf = buf->next;
				}
			}
		}
#endif
		delete buffer;
		buffer = NULL;
	}
}
StreamState KCacheStream::write_direct(char *buf,int len)
{
	StreamState result = KHttpStream::write_all(buf,len);
	if (buffer) {
		buffer->write_direct(buf, len);
		kassert(disk_cache == NULL);
		CheckMemoryCacheSize();
		return result;
	}
#ifdef ENABLE_DISK_CACHE
	if (disk_cache && !disk_cache->Write(rq,obj, buf, len)) {
		delete disk_cache;
		disk_cache = NULL;		
	}
#endif
	xfree(buf);
	return result;
}

StreamState KCacheStream::write_all(const char *buf,int len)
{
	StreamState result = KHttpStream::write_all(buf,len);
	if (buffer) {
		buffer->write_all(buf, len);
		kassert(disk_cache == NULL);
		CheckMemoryCacheSize();
		return result;
	}
#ifdef ENABLE_DISK_CACHE
	if (disk_cache && !disk_cache->Write(rq, obj, buf, len)) {
		delete disk_cache;
		disk_cache = NULL;		
	}
#endif
	return result;
}
#ifdef ENABLE_DISK_CACHE
KDiskCacheStream *KCacheStream::NewDiskCache()
{
	kassert(!rq->IsSync());
	KDiskCacheStream *disk_cache = new KDiskCacheStream;
	if (!disk_cache->Open(rq, obj)) {
		delete disk_cache;
		return NULL;
	}
	return disk_cache;
}
#endif
