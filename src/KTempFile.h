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
	//初始化,length是已经长度，如果为-1,则长度未知
	void init(INT64 length);
	//bool write_all(const char *buf, int len);
	int write(const char *buf, int len);
	char *writeBuffer(int &size);
	bool writeSuccess(int got);
	INT64 switchRead();
	//重新读
	void resetRead();
	int readBuffer(char *buf,int size);
	char *readBuffer(int &size);
	//返回true,继续读，否则读完了
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
	/////////////////post相关/////////////////////////
	char *getPostBuffer(int &size);
	kev_result readPostResult(KHttpRequest *rq,int got);
	kev_result read_post_end(KHttpRequest *rq);
	//对post数据过滤，需要建立一个post的流机制
	//KWStream *post_stream;
	//有post_stream时才有post_stream_buffer
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
	//总共还要从网络上读的数据
	INT64 length;
	//目前已有长度
	INT64 total_size;
	std::string file;
};
//读post数据到临时文件
kev_result stageTempFileReadPost(KHttpRequest *rq,AfterPostHandle handle,void *arg);
KTHREAD_FUNCTION clean_tempfile_thread(void *param);
#endif
#endif
