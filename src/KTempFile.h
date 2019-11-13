#ifndef KTEMPFILE_H
#define KTEMPFILE_H
#include "global.h"
#include <stdio.h>
#include <string>
#include "kforwin32.h"
#include "ksapi.h"
#include "KReadWriteBuffer.h"
#include "KFileName.h"
#include "kselector.h"
#include "KHttpStream.h"
#ifdef ENABLE_TF_EXCHANGE
#define TEMPFILE_POST_CHUNK_SIZE     8192
class KHttpRequest;
typedef kev_result (*AfterPostHandle)(KHttpRequest *rq,void *arg);
class KTempFilePostContext
{
public:
	KTempFilePostContext()
	{
		memset(this,0,sizeof(KTempFilePostContext));
	}
	~KTempFilePostContext()
	{
		if (post_stream) {
			delete post_stream;
		}
		if (buffer) {
			xfree(buffer);
		}
	}
	KWStream *post_stream;
	char *buffer;
	AfterPostHandle handle;
	void *arg;
};
class KTempFile : public KWStream
{
public:
	KTempFile();
	~KTempFile();
	//��ʼ��,length���Ѿ����ȣ����Ϊ-1,�򳤶�δ֪
	void init(INT64 length);
	//bool write_all(const char *buf, int len);
	int write(const char *buf, int len);
	char *writeBuffer(int &size);
	bool writeSuccess(int got);
	INT64 switchRead();
	//���¶�
	void resetRead();
	int readBuffer(char *buf,int size);
	char *readBuffer(int &size);
	//����true,�����������������
	bool readSuccess(int got);
	bool checkLast(int got)
	{
		if (got == 0) {
			return true;
		}
		if (length == -1) {
			return false;
		}
		return total_size + got >= length;
	}
	INT64 getSize()
	{
		return total_size;
	}
	/////////////////post���/////////////////////////
	char *getPostBuffer(int &size);
	kev_result readPostResult(KHttpRequest *rq,int got);
	kev_result read_post_end(KHttpRequest *rq);
	//��post���ݹ��ˣ���Ҫ����һ��post��������
	//KWStream *post_stream;
	//��post_streamʱ����post_stream_buffer
	//char *post_stream_buffer;
	StreamState writePostSuccess(int got);
	kev_result startPost(KHttpRequest *rq,AfterPostHandle handle,void *arg);
	//////////////////////////////////////////////////
	KTempFilePostContext *post_ctx;
private:
	bool HasPostStream()
	{
		return post_ctx && post_ctx->post_stream;
	}
	bool IsLengthUnknow()
	{
		return length == -1;
	}
	void adjustPostContentLength(KHttpRequest *rq);
	bool openFile();
	bool dumpOutBuffer();
	bool dumpInBuffer();
	KFile fp;
	KReadWriteBuffer buffer;
	//�ܹ���Ҫ�������϶�������
	INT64 length;
	//Ŀǰ���г���
	INT64 total_size;
	std::string file;
};
//��post���ݵ���ʱ�ļ�
kev_result stageTempFileReadPost(KHttpRequest *rq,AfterPostHandle handle,void *arg);
KTHREAD_FUNCTION clean_tempfile_thread(void *param);
#endif
#endif
