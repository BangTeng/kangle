#include<vector>
#include "KCacheStream.h"
#include "do_config.h"
#include "KCache.h"

KCacheStream::KCacheStream(KWStream *st,bool autoDelete) : KHttpStream(st,autoDelete)
{
	obj = NULL;
}
StreamState KCacheStream::write_end()
{
	if (!TEST(obj->index.flags,ANSW_NO_CACHE)) {
		set_buffer_obj(&buffer,obj);		
	}
	return KHttpStream::write_end();
}
StreamState KCacheStream::write_direct(char *buf,int len)
{
	StreamState result = KHttpStream::write_all(buf,len);
	if(!TEST(obj->index.flags,ANSW_NO_CACHE)){
		buffer.write_direct(buf,len);
		if(!result || buffer.getLen() > conf.max_cache_size){
			buffer.clean();
			SET(obj->index.flags,ANSW_NO_CACHE);
			return result;
		}
	} else {
		xfree(buf);
	}
	return result;
}
StreamState KCacheStream::write_all(const char *buf,int len)
{
	StreamState result = KHttpStream::write_all(buf,len);
	if(!TEST(obj->index.flags,ANSW_NO_CACHE)){
		buffer.write_all(buf,len);
		if(!result || buffer.getLen() > conf.max_cache_size){
			buffer.clean();
			SET(obj->index.flags,ANSW_NO_CACHE);
			return result;
		}
	}
	return result;
}

#if 0
KDiskCacheStream::KDiskCacheStream(KWStream *st, bool autoDelete) : KHttpStream(st, autoDelete)
{
	obj = NULL;
}
StreamState KDiskCacheStream::write_end()
{
	if (obj->data->fp) {
		SET(obj->index.flags, OBJ_IS_READY);
	}
	return KHttpStream::write_end();
}
StreamState KDiskCacheStream::write_all(const char *buf, int len)
{
	if (obj->data->fp) {
		if (len != obj->data->fp->write(buf, len)) {
			SET(obj->index.flags, FLAG_DEAD);
			delete obj->data->fp;
			obj->data->fp = NULL;
		}
	}
	return KHttpStream::write_all(buf, len);
}
void KDiskCacheStream::init(KHttpObject *obj)
{
	assert(obj);
	this->obj = obj;

	char *filename = obj->getFileName();
	if (filename == NULL) {
		return;
	}
	KFile *fp = new KFile;
	if (!fp->open(filename, fileWrite)) {
		free(filename);
		delete fp;
		return;
	}
	free(filename);
	obj->data->type = SAVING_OBJECT;
	obj->data->fp = fp;	
	SET(obj->index.flags, FLAG_IN_DISK| FLAG_BIG_OBJECT);
	int body_start = obj->saveHead(fp);
	if (body_start > 0) {
		fp->seek(body_start, seekBegin);
	}
}
#endif
