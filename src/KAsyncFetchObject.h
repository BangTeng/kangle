#ifndef KASYNCFETCHOBJECT_H
#define KASYNCFETCHOBJECT_H
#include "KSocketBuffer.h"
#include "KUpstreamSelectable.h"
#include "KHttpObject.h"
#include "KSelector.h"
#include "KSelectorManager.h"
enum Parse_Result
{
	Parse_Failed,
	Parse_Success,
	Parse_Continue
};
/**
* 异步调用扩展，所以支持异步调用的扩展从该类继承
*/
class KAsyncFetchObject : public KFetchObject
{
public:
	KAsyncFetchObject()
	{
		client = NULL;
		header = NULL;
		hot = NULL;
		current_size = 0;
		badStage = BadStage_Connect;
		buffer = NULL;
	}
	void close(KHttpRequest *rq)
	{
		if (client) {
			assert(!client->is_upstream_locked());
			if (TEST(rq->filter_flags,RF_UPSTREAM_NOKA) 
				|| !rq->ctx->upstream_connection_keep_alive) {
				lifeTime = -1;
			}
			client->gc(lifeTime);
			client = NULL;
		}
		KFetchObject::close(rq);
	}
	virtual ~KAsyncFetchObject()
	{
		assert(client==NULL);	
		if (client) {
			client->gc(-1);
		}
		if (header) {
			free(header);
		}
		if (buffer) {
			delete buffer;
		}
	}
	//期望中的完成，长连接中用于标识此连接还可用
	virtual void expectDone(KHttpRequest *rq)
	{
#ifndef NDEBUG
		rq->ctx->upstream_expected_done = true;
#endif
	}
#ifdef ENABLE_REQUEST_QUEUE
	bool needQueue()
	{
		return true;
	}
#endif
	void open(KHttpRequest *rq);
	void sendHead(KHttpRequest *rq);
	void readBody(KHttpRequest *rq);

	void handleReadBody(KHttpRequest *rq,int got);
	void handleReadHead(KHttpRequest *rq,int got);
	void handleSendHead(KHttpRequest *rq,int got);
	void handleHttp2ReadHead(KHttpRequest *rq,int got);
	void handleSendPost(KHttpRequest *rq,int got);
	void handleReadPost(KHttpRequest *rq,int got);
	//得到post读缓冲，发送到upstream
	void getPostRBuffer(KHttpRequest *rq,LPWSABUF buf,int &bufCount)
	{
		//新版已经简化流程，之前就把预加载的数据发送到buffer里面了。
		assert(rq->pre_post_length==0);
		buffer->getRBuffer(buf,bufCount);
	}
	int check_chunk_stream(KHttpRequest *rq,const char *buf,int got,bool &is_end) {
		int orig_len = got;
		KDechunkEngine *dechunk = rq->getDechunkEngine();
		const char *piece;
		int piece_len;
		while (got>0) {
			dechunk_status status = dechunk->dechunk(&buf,got,&piece,piece_len);
			switch (status) {
			case dechunk_end:
			case dechunk_failed:
				is_end = true;
			case dechunk_continue:
				return orig_len - got;
			default:
				break;
			}
		}
		return orig_len - got;
	}
	//得到post写缓冲，从client接收post数据
	char *getPostWBuffer(KHttpRequest *rq,int &len)
	{
		char *buf = buffer->getWBuffer(len);
		if (!rq->ctx->connection_upgrade && rq->left_read>=0) {			
			len = (int)(MIN((INT64)len,rq->left_read));
			assert(len>0);
		}
		return buf;
	}
	//得到head缓冲，从upstream读head
	char *getHeadRBuffer(KHttpRequest *rq,int &len)
	{
		if (header==NULL) {
			len = current_size = NBUFF_SIZE;
			header = (char *)malloc(current_size);
			hot = header;
			return hot;
		}
		assert(hot);
		unsigned used = (unsigned)(hot - header);
		assert(used<=current_size);
		if (used>=current_size) {
			int new_size = current_size * 2;
			char *n = (char *)malloc(2 * current_size);
			memcpy(n,header,current_size);
			adjustBuffer(n - header);
			free(header);
			header = n;
			hot = header + current_size;
			current_size = new_size;
		}
		len = current_size - used;
		return hot;
	}
	//得到body缓冲,从upstream读
	char *getBodyBuffer(KHttpRequest *rq,int &len)
	{
		//读body的时候，用回header的缓冲
		if (header == NULL) {
			len = current_size = NBUFF_SIZE;
			header = (char *)malloc(current_size);		
		}
		len = current_size;
		assert(len>0);
		return header;
	}
	KUpstreamSelectable *getSelectable()
	{
		return client;
	}
	KClientSocket *getSocket()
	{
		return client->socket;
	}
	bool try_pre_load_body(KHttpRequest *rq);
	void connect_result(KHttpRequest *rq, bool half_connection);
	void connectCallBack(KHttpRequest *rq,KUpstreamSelectable *client,bool half_connection = true);
	void handleUpstreamError(KHttpRequest *rq,int error,const char *msg,int last_got);
	void handleConnectResult(KHttpRequest *rq,int got);
	void shutdown(KHttpRequest *rq);
	KSocketBuffer *buffer;
	KUpstreamSelectable *client;
	BadStage badStage;
protected:
	void sendHeadSuccess(KHttpRequest *rq);
	void readHeadSuccess(KHttpRequest *rq);
	//header重新分配过时要重新调整偏移量
	virtual void adjustBuffer(INT64 offset)
	{
	}
	int lifeTime;
	char *header;
	char *hot;
	unsigned current_size;
	//创建发送头到buffer中。
	virtual void buildHead(KHttpRequest *rq) = 0;
	//解析head
	virtual Parse_Result parseHead(KHttpRequest *rq,char *data,int len) = 0;
	//创建post数据到buffer中。
	virtual void buildPost(KHttpRequest *rq)
	{
	}
	//检查是否还要继续读body,一般长连接需要。
	//如果本身有content-length则不用该函数
	virtual bool checkContinueReadBody(KHttpRequest *rq)
	{
		return true;
	}
	//读取body数据,返回 
	virtual char *nextBody(KHttpRequest *rq,int &len) = 0;
	//解析body
	virtual Parse_Result parseBody(KHttpRequest *rq,char *data,int len) = 0;
private:
	void read_hup(KHttpRequest *rq);	
	void continueReadBody(KHttpRequest *rq);
	void readPost(KHttpRequest *rq);
	void sendPost(KHttpRequest *rq);
	void retryOpen(KHttpRequest *rq);
	void startReadHead(KHttpRequest *rq);
};

#endif
