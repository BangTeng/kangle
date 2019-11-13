#ifndef KASYNCFETCHOBJECT_H
#define KASYNCFETCHOBJECT_H
#include "KSocketBuffer.h"
#include "KUpstream.h"
#include "KHttpObject.h"
#include "kselector.h"
#include "KHttpResponseParser.h"
#include "KHttpParser.h"
enum Parse_Result
{
	Parse_Failed,
	Parse_Success,
	Parse_Continue
};
/**
* �첽������չ������֧���첽���õ���չ�Ӹ���̳�
*/
class KAsyncFetchObject : public KFetchObject
{
public:
	KAsyncFetchObject()
	{
		client = NULL;
		badStage = BadStage_Connect;
		buffer = NULL;
		memset(&parser, 0, sizeof(parser));
		memset(&us_buffer, 0, sizeof(us_buffer));
	}
	void close(KHttpRequest *rq)
	{
		if (client) {
			assert(!client->IsLocked());
			if (TEST(rq->filter_flags,RF_UPSTREAM_NOKA) 
				|| !rq->ctx->upstream_connection_keep_alive
				|| !rq->ctx->upstream_expected_done) {
				parser_ctx.keep_alive_time_out = -1;
			}
			client->Gc(parser_ctx.keep_alive_time_out,parser_ctx.first_body_time);
			client = NULL;
		}
		KFetchObject::close(rq);
	}
	virtual ~KAsyncFetchObject()
	{
		assert(client==NULL);	
		if (client) {
			client->Gc(-1,0);
		}
		if (buffer) {
			delete buffer;
		}
		if (us_buffer.buf) {
			xfree(us_buffer.buf);
		}
	}
	//�����е���ɣ������������ڱ�ʶ�����ӻ�����
	virtual void expectDone(KHttpRequest *rq)
	{
		rq->ctx->upstream_expected_done = 1;
	}
#ifdef ENABLE_REQUEST_QUEUE
	bool needQueue()
	{
		return true;
	}
#endif
	kev_result open(KHttpRequest *rq);
	kev_result sendHead(KHttpRequest *rq);
	kev_result readBody(KHttpRequest *rq);

	kev_result handleReadBody(KHttpRequest *rq,int got);
	kev_result handleReadHead(KHttpRequest *rq,int got);
	kev_result handleSendHead(KHttpRequest *rq,int got);
	kev_result handleHttp2ReadHead(KHttpRequest *rq,int got);
	kev_result handleSendPost(KHttpRequest *rq,int got);
	kev_result handleReadPost(KHttpRequest *rq,int got);
	//�õ�post�����壬���͵�upstream
	int getPostRBuffer(KHttpRequest *rq,LPWSABUF buf,int bufCount)
	{
		//�°��Ѿ������̣�֮ǰ�Ͱ�Ԥ���ص����ݷ��͵�buffer�����ˡ�
		//assert(rq->pre_post_length==0);
		return buffer->getReadBuffer(buf,bufCount);
	}
	//�õ�postд���壬��client����post����
	char *getPostWBuffer(KHttpRequest *rq,int &len)
	{
		char *buf = buffer->getWriteBuffer(len);
		if (!rq->ctx->connection_upgrade && rq->left_read>=0) {			
			len = (int)(MIN((INT64)len,rq->left_read));
			assert(len>0);
		}
		return buf;
	}
	//�õ�body����,��upstream��
	char *getUpstreamBuffer(int *len)
	{
		return ks_get_write_buffer(&us_buffer, len);
	}
	KUpstream *GetUpstream()
	{
		return client;
	}
	kev_result connect_result(KHttpRequest *rq, bool half_connection);
	kev_result connectCallBack(KHttpRequest *rq,KUpstream *client,bool half_connection = true);
	kev_result handleUpstreamError(KHttpRequest *rq,int error,const char *msg,int last_got);
	kev_result handleConnectResult(KHttpRequest *rq,int got);
	kev_result shutdown(KHttpRequest *rq);
	ks_buffer us_buffer;
	KSocketBuffer *buffer;
	KUpstream *client;
	BadStage badStage;
	friend class KHttp2;
protected:
	kev_result sendHeadSuccess(KHttpRequest *rq);
	kev_result readHeadSuccess(KHttpRequest *rq);
	void BuildChunkHeader();
	//��������ͷ��buffer�С�
	virtual void buildHead(KHttpRequest *rq) = 0;
	//����head
	virtual kgl_parse_result ParseHeader(KHttpRequest *rq,char **data, int *len);
	virtual StreamState ParseBody(KHttpRequest *rq, char **data, int *len);
	//����post���ݵ�buffer�С�
	virtual void buildPost(KHttpRequest *rq)
	{
	}
	//����Ƿ�Ҫ������body,һ�㳤������Ҫ��
	//���������content-length���øú���
	virtual bool checkContinueReadBody(KHttpRequest *rq)
	{
		return true;
	}
	KHttpResponseParser parser_ctx;
	khttp_parser parser;
private:
	kev_result ParseBody(KHttpRequest *rq);
	kev_result PushBodyResult(KHttpRequest *rq, StreamState result);
	void read_hup(KHttpRequest *rq);	
	kev_result continueReadBody(KHttpRequest *rq);
	kev_result readPost(KHttpRequest *rq);
	kev_result sendPost(KHttpRequest *rq);
	kev_result retryOpen(KHttpRequest *rq);
	kev_result startReadHead(KHttpRequest *rq);
	void InitUpstreamBuffer()
	{
		if (us_buffer.buf) {
			xfree(us_buffer.buf);
		}
		ks_buffer_init(&us_buffer, 8192);
	}
};
#endif
