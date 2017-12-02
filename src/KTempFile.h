#ifndef KTEMPFILE_H
#define KTEMPFILE_H
#include "global.h"
#include <stdio.h>
#include <string>
#include "forwin32.h"
#include "KSocketBuffer.h"
#include "KFile.h"
#include "KHttpStream.h"
#include "malloc_debug.h"
#ifdef ENABLE_TF_EXCHANGE
#define TEMPFILE_POST_CHUNK_SIZE     8192
class KHttpRequest;
typedef void (*AfterPostHandle)(KHttpRequest *rq,void *arg);
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
class KTempFile
{
public:
	KTempFile();
	~KTempFile();
	//��ʼ��,length���Ѿ����ȣ����Ϊ-1,�򳤶�δ֪
	void init(INT64 length);
	bool writeBuffer(KHttpRequest *rq,const char *buf,int len);
	char *writeBuffer(int &size);
	bool writeSuccess(KHttpRequest *rq,int got);
	INT64 switchRead();
	//���¶�
	void resetRead();
	int readBuffer(char *buf,int size);
	char *readBuffer(int &size);
	//����true,�����������������
	bool readSuccess(int got);
	bool isWrite()
	{
		return writeModel;
	}
	bool checkLast(int got)
	{
		return total_size + got >= length;
	}
	INT64 getSize()
	{
		return total_size;
	}
	/////////////////post���/////////////////////////
	char *getPostBuffer(int &size);
	bool readPostResult(KHttpRequest *rq,int got);
	bool try_pre_load_body(KHttpRequest *rq);
	void read_post_end(KHttpRequest *rq);
	//��post���ݹ��ˣ���Ҫ����һ��post��������
	//KWStream *post_stream;
	//��post_streamʱ����post_stream_buffer
	//char *post_stream_buffer;
	StreamState writePostSuccess(KHttpRequest *rq,int got);
	void startPost(KHttpRequest *rq,AfterPostHandle handle,void *arg);
	//////////////////////////////////////////////////
	KTempFilePostContext *post_ctx;
private:
	void adjustPostContentLength(KHttpRequest *rq);
	bool openFile(KHttpRequest *rq);
	bool dumpOutBuffer();
	bool dumpInBuffer();
	KFile fp;
	KSocketBuffer buffer;
	//�ܹ���Ҫ�������϶�������
	INT64 length;
	//Ŀǰ���г���
	INT64 total_size;
	std::string file;
	bool writeModel;
};
//��post���ݵ���ʱ�ļ�
void stageTempFileReadPost(KHttpRequest *rq,AfterPostHandle handle,void *arg);
FUNC_TYPE FUNC_CALL clean_tempfile_thread(void *param);
#endif
#endif
