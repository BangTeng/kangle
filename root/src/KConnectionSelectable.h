#ifndef KCONNECTIONSELECTABLE_H
#define KCONNECTIONSELECTABLE_H
#include "KSelectable.h"
#include "KVirtualHostContainer.h"
#include "KMemPool.h"
class KHttp2;
class KHttpRequest;
class KServer;
class KSubVirtualHost;
#ifdef KSOCKET_SSL
class KSSLSniContext
{
public:
	KSSLSniContext()
	{
		svh = NULL;
		result = query_vh_unknow;
	}
	~KSSLSniContext();
	query_vh_result result;
	KSubVirtualHost *svh;
};
#endif
class KConnectionSelectable : public KSelectable
{
public:
	KConnectionSelectable(KClientSocket *socket)
	{
		memset(static_cast<KSelectable *>(this), 0, sizeof(KSelectable));
		this->socket = socket;
		fd = socket;
#ifdef ENABLE_HTTP2
		http2 = NULL;
#endif
		ls = NULL;
#ifdef KSOCKET_SSL
		sni = NULL;
#endif
		pool = NULL;
	}
	kgl_pool_t *get_pool()
	{
		if (pool == NULL) {
			pool = kgl_create_pool(4096);
		}
		return pool;
	}
#ifdef KSOCKET_SSL
	void resultSSLShutdown(int got);
	query_vh_result useSniVirtualHost(KHttpRequest *rq);
#endif
	/* 同步读 */
	int read(KHttpRequest *rq,char *buf,int len);
	/* 同步写 */
	int write(KHttpRequest *rq,LPWSABUF buf,int bufCount);
	bool write_all(KHttpRequest *rq, const char *buf, int len);
	/* 延时异步读 */
	void delayRead(KHttpRequest *rq,resultEvent result,bufferEvent buffer,int msec);
	/* 延时异步写 */
	void delayWrite(KHttpRequest *rq,resultEvent result,bufferEvent buffer,int msec);
	/* 异步读 */
	bool read(KHttpRequest *rq,resultEvent result,bufferEvent buffer,int list=KGL_LIST_RW);
	/* 异步写 */
	void write(KHttpRequest *rq,resultEvent result,bufferEvent buffer);
	void read_hup(KHttpRequest *rq,resultEvent result);
	void shutdown(KHttpRequest *rq);
	//void removeSocket();

	void next(KHttpRequest *rq,resultEvent result);	
	void removeRequest(KHttpRequest *rq,bool add_sync);
	//return header len
	int startResponse(KHttpRequest *rq,INT64 body_len);
	void endResponse(KHttpRequest *rq,bool keep_alive);
	

	KSocket *getSocket()
	{
		return socket;
	}
	void real_destroy();
	void release(KHttpRequest *rq);
	/*
	 * server是原始socket,
	 * 一般情况是一样的，如果在ssi里面内部调用时就不一样了。
	 */
	KClientSocket *socket;
#ifdef ENABLE_HTTP2
	KHttp2 *http2;
	friend class KHttp2;
#endif
	//请求所在侦听
	KServer *ls;
#ifdef KSOCKET_SSL
	KSSLSniContext *sni;
#endif
	kgl_pool_t *pool;
protected:
	virtual ~KConnectionSelectable();
	void internalRemoveRequest(bool add_sync) {
		selector->removeSocket(this);
		if (add_sync) {
			selector->addList(this, KGL_LIST_SYNC);
		} else {
			selector->removeList(this);
		}
	}
};

#endif
