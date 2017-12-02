#ifndef KCHUNKED_H
#define KCHUNKED_H
#include "global.h"
#include "KSendable.h"
#include "KHttpStream.h"
/*
������С
bufferǰ��Ҫ����5���ֽڴ�С,��chunkͷ,����Ҫ����2���ֽ�����β���ṹ����:

xxxx\r\n******buffer data******\r\n

ע�����CHUNKED_BUFFER_SIZE��ֵ���ô���4096ʱ,CHUNKED_BUFFER_HEAD_SIZE��Ҫ���6
���������
*/
#define CHUNKED_BUFFER_HEAD_SIZE    5
#define CHUNKED_BUFFER_SIZE         4096
#define CHUNKED_BUFFER_END_SIZE     2
/*
���buffer���ݣ����ܴ�С����ǰ��ͷ�ͺ����β
*/
#define MAX_BUFFER_SIZE             (CHUNKED_BUFFER_SIZE-CHUNKED_BUFFER_HEAD_SIZE-CHUNKED_BUFFER_END_SIZE)
/*
�����KDeChunked���෴�����Կ鷢��
*/
class KChunked : public KHttpStream
{
public:
	KChunked(KWStream *st,bool autoDelete);
	~KChunked();
	StreamState write_all(const char *buf,int size);
	StreamState write_end();
private:
	bool firstPackage;
};
#endif
