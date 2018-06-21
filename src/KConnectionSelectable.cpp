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
void delay_read(void *arg,int got)
{
	kgl_delay_io *io = (kgl_delay_io *)arg;
	io->c->read(io->rq,io->result,io->buffer);
	delete io;
}
void delay_write(void *arg,int got)
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
bool KConnectionSelectable::is_event(KHttpRequest *rq,uint16_t flag)
{
#ifdef ENABLE_HTTP2
	if (rq->http2_ctx) {
		if (TEST(flag, STF_REVENT) && rq->http2_ctx->read_wait != NULL) {
			return true;
		}
		if (TEST(flag, STF_WEVENT) && rq->http2_ctx->write_wait != NULL) {
			return true;
		}
		return false;
	}
#endif
	flag = (flag&STF_EVENT);
	return TEST(st_flags, flag) > 0;
}
bool KConnectionSelectable::is_locked(KHttpRequest *rq)
{
#ifdef ENABLE_HTTP2
	if (rq->http2_ctx) {
		if (rq->http2_ctx->read_wait != NULL) {
			return true;
		}
		if (rq->http2_ctx->write_wait != NULL && rq->http2_ctx->write_wait->buffer != NULL) {
			return true;
		}
		return false;
	}
#endif
	return TEST(st_flags, STF_LOCK) > 0;
}
KConnectionSelectable::~KConnectionSelectable()
{
	//printf("st=[%p] deleted\n", static_cast<KSelectable *>(this));
	assert(TEST(st_flags, STF_LOCK)==0);
	assert(queue.next == NULL);
	assert(queue.prev == NULL);
	if (socket) {
		/**
		* 这里为什么要调用一次removeSocket呢？
		* 一般来讲，在linux下面,一个socket close了,epoll自动会删除相关事件。
		* 但如果此时fork了，此socket在一瞬间被其它子进程共享，即使设置了close_on_exec,但此fd在fork之后，exec之前还是处于未关闭状态。
		* 此时虽close此socket,epoll还是会返回相关事件。
		* 除非确保当前进程不会fork.
		*/
		if (selector) {
			selector->removeSocket(this);
		}
		delRequest(this);
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
void KConnectionSelectable::read(KHttpRequest *rq,resultEvent result,bufferEvent buffer)
{
#ifdef ENABLE_HTTP2
	if (http2) {
		http2->read(rq->http2_ctx,result,buffer,rq);
		return;
	}
#endif
	async_read(rq,result,buffer);
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
	assert(selector->is_same_thread());
#ifdef ENABLE_HTTP2
	if (http2) {
		http2->write(rq->http2_ctx,result,buffer,rq);
		return;
	}
#endif
	async_write(rq,result,buffer);
}
void KConnectionSelectable::read_hup(KHttpRequest *rq,resultEvent result)
{
#ifdef ENABLE_HTTP2
	if (http2) {
		http2->read_hup(rq->http2_ctx, result, rq);
		return;
	}
#endif
#ifdef WORK_MODEL_SIMULATE
	if (TEST(rq->workModel, WORK_MODEL_SIMULATE)) {
		return;
	}
#endif
	selector->read_hup(this,result,NULL,rq);
}
void KConnectionSelectable::add_sync(KHttpRequest *rq)
{
	assert(rq->c->selector->is_same_thread());
#ifdef ENABLE_HTTP2
	if (http2) {
		assert(rq->http2_ctx->read_wait == NULL);
		return;
	}
#endif
	selector->removeSocket(this);
	selector->add_list(this, KGL_LIST_SYNC);	
}
void KConnectionSelectable::remove_sync(KHttpRequest *rq)
{
	assert(rq->c->selector->is_same_thread());
#ifdef ENABLE_HTTP2
	if (http2) {
		return;
	}
#endif
	selector->remove_list(this);
}
void KConnectionSelectable::remove_read_hup(KHttpRequest *rq)
{
#ifdef ENABLE_HTTP2
	if (http2) {
		http2->remove_read_hup(rq->http2_ctx);
		return;
	}
#endif
	selector->remove_read_hup(this);
	return;
}
void KConnectionSelectable::shutdown(KHttpRequest *rq)
{
#ifdef ENABLE_HTTP2
	if (http2) {
		http2->shutdown(rq->http2_ctx);
		return;
	}
#endif
	shutdown_socket();
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
	assert(selector->is_same_thread());
	kgl_delay_io *io = new kgl_delay_io;
	io->c = this;
	io->rq = rq;
	io->result = result;
	io->buffer = buffer;
	selector->add_timer(delay_read,io,msec, rq->c);
}
void KConnectionSelectable::delayWrite(KHttpRequest *rq,resultEvent result,bufferEvent buffer,int msec)
{
	assert(selector->is_same_thread());
	kgl_delay_io *io = new kgl_delay_io;
	io->c = this;
	io->rq = rq;
	io->result = result;
	io->buffer = buffer;
	selector->add_timer(delay_write,io,msec,rq->c);
}
int KConnectionSelectable::start_response(KHttpRequest *rq,INT64 body_len)
{
#ifdef ENABLE_HTTP2
	if (http2) {
		assert(rq->http2_ctx);
		if (TEST(rq->flags, RQ_SYNC)) {
			return http2->sync_send_header(rq->http2_ctx, body_len);
		}
		return http2->send_header(rq->http2_ctx, body_len);
	}
#endif
	socket->set_delay();
	return 0;
}
void KConnectionSelectable::end_response(KHttpRequest *rq,bool keep_alive)
{
#ifdef ENABLE_HTTP2
	if (http2) {
		SET(rq->flags,RQ_CONNECTION_CLOSE);
#ifndef NDEBUG
	/*
		unsigned char md5[16];
		char md5_str[33];
		KMD5Final(md5, &rq->http2_ctx->md5);
		make_digest(md5_str, md5);
		klog(KLOG_WARNING, "%s %s md5=[%s]\n",rq->getClientIp(), rq->url->path, md5_str);
	*/
#endif
		http2->write_end(rq->http2_ctx);
		return;
	}
#endif
	if (keep_alive) {
		socket->set_nodelay();
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
	ssl_status status = sslSocket->ssl_shutdown();
#ifdef ENABLE_KSSL_BIO
	if (status != ret_error && BIO_pending(sslSocket->ssl_bio[WRITE_PIPE].bio)>0) {
		async_write(this, ::resultSSLShutdown, NULL);
		return;
	}	
#endif
	switch (status) {
	case SSL_ERROR_WANT_READ:
#ifndef ENABLE_KSSL_BIO
		clear_flag(STF_RREADY);
#endif
		async_read(this, ::resultSSLShutdown, NULL);
		return;
	case SSL_ERROR_WANT_WRITE:
#ifndef ENABLE_KSSL_BIO
		clear_flag(STF_WREADY);
#endif
		async_write(this, ::resultSSLShutdown, NULL);
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
#ifdef KSOCKET_SSL
		if (socket && isSSL()) {
			resultSSLShutdown(0);
			return;
		}
#endif
		assert(queue.next==NULL);
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
