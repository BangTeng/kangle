#ifndef KTEMPFILESTREAM_H
#define KTEMPFILESTREAM_H
#include "KHttpStream.h"
class KHttpRequest;
#ifdef ENABLE_TF_EXCHANGE
class KTempFileStream : public KHttpStream
{
public:
	KTempFileStream(KHttpRequest *rq);
	StreamState write_all(const char *buf, int len);
protected:
	int write(const char *buf, int len);
private:
	KHttpRequest *rq;
};
#endif
#endif
