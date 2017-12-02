#include "KConnectionSelectable.h"
#include "KSelector.h"
#include "KHttp2.h"
#include "KServer.h"
#include "KSubVirtualHost.h"
#include "KSelectorManager.h"
struct kgl_delay_io
{
	KConnectionSelectable *c;
	KHttpRequest *rq;
	bufferEvent buffer;
	resultEvent result;
};
void WINAPI delay_read(void *arg)
{
	kgl_delay_io *io = (kgl_delay_io *)arg;
	io->c->read(io->rq,io->result,io->buffer);
	delete io;
}
void WINAPI delay_write(void *arg)
{
	kgl_delay_io *io = (kgl_delay_io *)arg;
	io->c->write(io->rq,io->result,io->buffer);
	delete io;
}
#ifdef KSOCKET_SSL
void resultSSLShutdown(void *arg,int got)
{
	KConnectionSelectable *c = (KConnectionSelectable *)arg;
	c->resultSSLShutdown(got);
}
#endif
KConnectionSelectable::~KConnectionSelectable()
{
	assert(queue.next == NULL);
	assert(queue.prev == NULL);
	if (socket) {
		if (selector) {
			/**
			* 这里为什么要调用一次removeSocket呢？
			* 一般来讲，在linux下面,一个socket close了。epoll自动会删除。
			* 但如果存在多进程的时候，此socket在一瞬间被其它进程共享，
			* 则close此socket,epoll还是会返回相关事件。
			*/
			selector->removeSocket(this);
		}
		delRequest(st_flags,socket);
		socket->shutdown(SHUT_RDWR);
		delete socket;
	}
#ifdef ENABLE_HTTP2
	if (http2) {
		delete http2;
	}
#endif
	if (ls) {
		ls->release();
	}
#ifdef KSOCKET_SSL
	if (sni) {
		delete sni;
	}
#endif
	if (pool) {
		kgl_destroy_pool(pool);
	}
}
void KConnectionSelectable::release(KHttpRequest *rq)
{
#ifdef ENABLE_HTTP2
	if (http2) {
		http2->write_end(rq->http2_ctx);
		http2->release(rq->http2_ctx);
		return;
	}
#endif
	real_destroy();
}
bool KConnectionSelectable::read(KHttpRequest *rq,resultEvent result,bufferEvent buffer,int list)
{
#ifdef ENABLE_HTTP2
	if (http2) {
		http2->read(rq->http2_ctx,result,buffer,rq);
		return false;
	}
#endif
	return asyncRead(rq,result,buffer,list);
}
bool KConnectionSelectable::write_all(KHttpRequest *rq, const char *buf, int len)
{
	WSABUF b;
	while (len > 0) {
		b.iov_len = len;
		b.iov_base = (char *)buf;
		int got = write(rq, &b, 1);
		if (got <= 0) {
			return false;
		}
		len -= got;
		buf += got;
	}
	return true;
}
int KConnectionSelectable::write(KHttpRequest *rq,LPWSABUF buf,int bufCount)
{
#ifdef ENABLE_HTTP2
	if (http2) {
		return http2->write(rq->http2_ctx,buf,bufCount);
	}
#endif
	return socket->writev(buf,bufCount,isSSL());
}
void KConnectionSelectable::write(KHttpRequest *rq,resultEvent result,bufferEvent buffer)
{
#ifdef ENABLE_HTTP2
	if (http2) {
		http2->write(rq->http2_ctx,result,buffer,rq);
		return;
	}
#endif
	asyncWrite(rq,result,buffer);
}
void KConnectionSelectable::next(KHttpRequest *rq,resultEvent result)
{
#ifdef ENABLE_HTTP2
	if (http2) {
		selector->addTimer(NULL, result, rq, 0);
		return;
	}
#endif
	if (!selector->next(this,result,rq)) {
		selector->addTimer(NULL, result, rq, 0);
	}
}
void KConnectionSelectable::read_hup(KHttpRequest *rq,resultEvent result)
{
#ifdef ENABLE_HTTP2
	if (http2) {
		http2->read_hup(rq->http2_ctx, result, rq);
		return;
	}
#endif
	selector->addList(this, KGL_LIST_SYNC);
	if (!selector->read_hup(this,result,NULL,rq)) {
		selector->removeSocket(this);
	}
}
void KConnectionSelectable::removeRequest(KHttpRequest *rq,bool add_sync)
{
#ifdef ENABLE_HTTP2
	if (http2) {
		assert(rq->http2_ctx->read_wait == NULL);
		//assert(rq->http2_ctx->write_wait == NULL);
		http2->remove_event(rq->http2_ctx);
		return;
	}
#endif
	internalRemoveRequest(add_sync);
}
void KConnectionSelectable::shutdown(KHttpRequest *rq)
{
#ifdef ENABLE_HTTP2
	if (http2) {
		http2->shutdown(rq->http2_ctx);
		return;
	}
#endif
	socket->shutdown(SHUT_RDWR);
#ifdef _WIN32
	socket->cancelIo();
#endif
}
int KConnectionSelectable::read(KHttpRequest *rq,char *buf,int len)
{
#ifdef ENABLE_HTTP2
	if (http2) {
		return http2->read(rq->http2_ctx,buf,len);
	}
#endif
	return socket->read(buf,len);
}
void KConnectionSelectable::delayRead(KHttpRequest *rq,resultEvent result,bufferEvent buffer,int msec)
{
	kgl_delay_io *io = new kgl_delay_io;
	io->c = this;
	io->rq = rq;
	io->result = result;
	io->buffer = buffer;
	removeRequest(rq,true);
	selector->addTimer(NULL,delay_read,io,msec);
}
void KConnectionSelectable::delayWrite(KHttpRequest *rq,resultEvent result,bufferEvent buffer,int msec)
{
	kgl_delay_io *io = new kgl_delay_io;
	io->c = this;
	io->rq = rq;
	io->result = result;
	io->buffer = buffer;
	removeRequest(rq,true);
	selector->addTimer(NULL,delay_write,io,msec);
}
int KConnectionSelectable::startResponse(KHttpRequest *rq,INT64 body_len)
{
#ifdef ENABLE_HTTP2
	if (http2) {
		assert(rq->http2_ctx);
		return http2->send_header(rq->http2_ctx, body_len);
	}
#endif
	socket->setdelay();
	return 0;
}
void KConnectionSelectable::endResponse(KHttpRequest *rq,bool keep_alive)
{
#ifdef ENABLE_HTTP2
	if (http2) {
		SET(rq->flags,RQ_CONNECTION_CLOSE);
		http2->write_end(rq->http2_ctx);
		return;
	}
#endif
	if (keep_alive) {
		socket->setnodelay();
		SET(rq->workModel,WORK_MODEL_KA);
	}
}
#ifdef KSOCKET_SSL
query_vh_result KConnectionSelectable::useSniVirtualHost(KHttpRequest *rq)
{
	assert(rq->svh==NULL);
	rq->svh = sni->svh;
	sni->svh = NULL;
	query_vh_result ret = sni->result;
	delete sni;
	sni = NULL;
	return ret;
}
void KConnectionSelectable::resultSSLShutdown(int got)
{
	if (got<0) {
		delete this;
		return;
	}
	assert(socket);
	KSSLSocket *sslSocket = static_cast<KSSLSocket *>(socket);
	SSL *ssl = sslSocket->getSSL();
	if (ssl==NULL) {
		delete this;
		return;
	}
	int n = SSL_shutdown(ssl);
	if (n==1) {
		delete this;
		return;
	}
	int err = SSL_get_error(ssl,n);
	switch (err) {
	case SSL_ERROR_WANT_READ:
		if (!selector->read(this, ::resultSSLShutdown, NULL, this)) {
			delete this;
		}
		return;
	case SSL_ERROR_WANT_WRITE:
		if (!selector->write(this, ::resultSSLShutdown, NULL, this)) {
			delete this;
		}
		return;
	default:
		delete this;
		return;
	}
}
#endif
void KConnectionSelectable::real_destroy()
{
	if (selector) {		
		selector->removeList(this);
		if (socket) {
			selector->removeSocket(this);
#ifdef KSOCKET_SSL
#ifndef MALLOCDEBUG
			if (isSSL()) {
				resultSSLShutdown(0);
				return;
			}
#endif
#endif
		}
	}
	delete this;
}
#ifdef KSOCKET_SSL
KSSLSniContext::~KSSLSniContext()
{
	if (svh) {
		svh->release();
	}
}
#endif
