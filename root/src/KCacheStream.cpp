#include<vector>
#include "KCacheStream.h"
#include "do_config.h"

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

