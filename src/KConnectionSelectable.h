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
		bind_socket(socket);
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
	bool is_event(KHttpRequest *rq, uint16_t flag);

	bool is_locked(KHttpRequest *rq);

	int read(KHttpRequest *rq,char *buf,int len);

	int write(KHttpRequest *rq,LPWSABUF buf,int bufCount);
	bool write_all(KHttpRequest *rq, const char *buf, int len);

	void delayRead(KHttpRequest *rq,resultEvent result,bufferEvent buffer,int msec);

	void delayWrite(KHttpRequest *rq,resultEvent result,bufferEvent buffer,int msec);

	void read(KHttpRequest *rq,resultEvent result,bufferEvent buffer);

	void write(KHttpRequest *rq,resultEvent result,bufferEvent buffer);
	void read_hup(KHttpRequest *rq,resultEvent result);
	void shutdown(KHttpRequest *rq);
	void remove_read_hup(KHttpRequest *rq);
	void add_sync(KHttpRequest *rq);
	void remove_sync(KHttpRequest *rq);
	//return header len
	int start_response(KHttpRequest *rq, INT64 body_len);
	void end_response(KHttpRequest *rq,bool keep_alive);
	

	KSocket *getSocket()
	{
		return socket;
	}
	void real_destroy();
	void release(KHttpRequest *rq);

	KClientSocket *socket;
#ifdef ENABLE_HTTP2
	KHttp2 *http2;
	friend class KHttp2;
#endif
	KServer *ls;
#ifdef KSOCKET_SSL
	KSSLSniContext *sni;
#endif
	kgl_pool_t *pool;
protected:
	virtual ~KConnectionSelectable();
};
void request_connection_broken(void *arg, int got);
#endif
