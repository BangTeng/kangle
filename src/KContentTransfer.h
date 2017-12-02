#ifndef KCONTENTTRANSFER_H
#define KCONTENTTRANSFER_H
#include "KHttpStream.h"
#include "KHttpRequest.h"

/*
内容变换
*/
class KContentTransfer : public KHttpStream
{
public:
	KContentTransfer(KWStream *st,bool autoDelete) : KHttpStream(st,autoDelete)
	{
		rq = NULL;
	}
	/*
	发送完了http头再调用这个init，来配置适当的输出流
	*/
	bool init(KHttpRequest *rq);
	StreamState write_all(const char *str,int len);
private:
	KHttpRequest *rq;
};
#endif
