#ifndef KCONTENTTRANSFER_H
#define KCONTENTTRANSFER_H
#include "KHttpStream.h"
#include "KHttpRequest.h"

/*
���ݱ任
*/
class KContentTransfer : public KHttpStream
{
public:
	KContentTransfer(KWStream *st,bool autoDelete) : KHttpStream(st,autoDelete)
	{
		rq = NULL;
	}
	/*
	��������httpͷ�ٵ������init���������ʵ��������
	*/
	bool init(KHttpRequest *rq);
	StreamState write_all(const char *str,int len);
private:
	KHttpRequest *rq;
};
#endif
