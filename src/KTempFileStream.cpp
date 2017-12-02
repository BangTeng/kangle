#include "KTempFileStream.h"
#include "KHttpRequest.h"
#ifdef ENABLE_TF_EXCHANGE
KTempFileStream::KTempFileStream(KHttpRequest *rq)
{
	this->rq = rq;
}
StreamState KTempFileStream::write_all(const char *buf, int len)
{
	if (!rq->tf->writeBuffer(rq,buf,len)) {
		return STREAM_WRITE_FAILED;
	}
	return STREAM_WRITE_SUCCESS;
}
int KTempFileStream::write(const char *buf, int len)
{
	if (!rq->tf->writeBuffer(rq,buf,len)) {
		return -1;
	}
	return len;
}
#endif
