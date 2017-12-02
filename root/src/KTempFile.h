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
	//初始化,length是已经长度，如果为-1,则长度未知
	void init(INT64 length);
	bool writeBuffer(KHttpRequest *rq,const char *buf,int len);
	char *writeBuffer(int &size);
	bool writeSuccess(KHttpRequest *rq,int got);
	INT64 switchRead();
	//重新读
	void resetRead();
	int readBuffer(char *buf,int size);
	char *readBuffer(int &size);
	//返回true,继续读，否则读完了
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
	/////////////////post相关/////////////////////////
	char *getPostBuffer(int &size);
	bool readPostResult(KHttpRequest *rq,int got);
	bool try_pre_load_body(KHttpRequest *rq);
	void read_post_end(KHttpRequest *rq);
	//对post数据过滤，需要建立一个post的流机制
	//KWStream *post_stream;
	//有post_stream时才有post_stream_buffer
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
	//总共还要从网络上读的数据
	INT64 length;
	//目前已有长度
	INT64 total_size;
	std::string file;
	bool writeModel;
};
//读post数据到临时文件
void stageTempFileReadPost(KHttpRequest *rq,AfterPostHandle handle,void *arg);
FUNC_TYPE FUNC_CALL clean_tempfile_thread(void *param);
#endif
#endif
