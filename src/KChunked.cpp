#include <string.h>
#include <stdio.h>
#include "KChunked.h"
#include "malloc_debug.h"
KChunked::KChunked(KWStream *st,bool autoDelete) : KHttpStream(st,autoDelete)
{
	firstPackage = true;
}
KChunked::~KChunked()
{

}
StreamState KChunked::write_all(const char *buf,int size)
{
	if (size<=0) {
		return STREAM_WRITE_SUCCESS;
	}
	char header[32];
	int size_len ;
	if(firstPackage){
		size_len = sprintf(header,"%x\r\n",size);
	} else {
		size_len = sprintf(header,"\r\n%x\r\n",size);
	}
	firstPackage = false;
	StreamState result = st->write_all(header,size_len);
	if(result!=STREAM_WRITE_SUCCESS){
		return result;
	}
	return st->write_all(buf,size);
}
StreamState KChunked::write_end()
{
	if (firstPackage) {
		KHttpStream::write_all("0\r\n\r\n",5);
	} else {
		KHttpStream::write_all("\r\n0\r\n\r\n",7);
	}
	return KHttpStream::write_end();
}
