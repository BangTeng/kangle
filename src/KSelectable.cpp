#include "KSelectable.h"
#include "KSelector.h"
#include "KSSLSocket.h"
#include "KPipeStream.h"
#include "time_utils.h"
#define MAXSENDBUF 16
void KSelectable::shutdown_socket()
{
	//printf("st=[%p] shutdown_socket\n", this);
	sock->shutdown(SHUT_RDWR);
#ifdef _WIN32
	sock->cancelIo();
#endif
}
void KSelectable::async_write(void *arg, resultEvent result, bufferEvent buffer)
{
	assert(selector->is_same_thread());
	//selector->addList(this, KGL_LIST_RW);
#ifdef ENABLE_KSSL_BIO
	if (isSSL()) {
		sslWrite(arg, result, buffer);
		return;
	}
#endif
	if (!selector->write(this, result, buffer, arg)) {
		result(arg, -1);
	}
}
#ifdef ENABLE_KSSL_BIO
void KSelectable::sslRead(void *arg, resultEvent result, bufferEvent buffer)
{
	KSSLSocket *ssl_socket = static_cast<KSSLSocket *>(getSocket());
	KSSLBIO *ssl_bio = &ssl_socket->ssl_bio[READ_PIPE];
	ssl_bio->buffer = buffer;
	ssl_bio->result = result;
	ssl_bio->arg = arg;
	ssl_bio->st = this;
	if (BIO_pending(ssl_bio->bio) > 0) {
		eventRead(arg, result, buffer);
		return;
	}
	if (!selector->read(this, resultSSLBIORead, bufferSSLBIORead, ssl_bio)) {
		result(arg, -1);
	}
}
void KSelectable::sslWrite(void *arg, resultEvent result, bufferEvent buffer)
{
	KSSLSocket *ssl_socket = static_cast<KSSLSocket *>(getSocket());
	KSSLBIO *ssl_bio = &ssl_socket->ssl_bio[WRITE_PIPE];
	ssl_bio->buffer = buffer;
	ssl_bio->result = result;
	ssl_bio->arg = arg;
	ssl_bio->got = 0;
	ssl_bio->st = this;
	if (buffer) {
		WSABUF recvBuf[MAXSENDBUF];
		memset(&recvBuf, 0, sizeof(recvBuf));
		int bufferCount = MAXSENDBUF;
		buffer(arg, recvBuf, bufferCount);
		ssl_bio->got = ssl_socket->writev(recvBuf, bufferCount, true);
		//printf("writev got=[%d]\n", ssl_bio->got);
	}
	if (BIO_pending(ssl_bio->bio) <= 0) {
		result(arg, ssl_bio->got);
		return;
	}
	if (!selector->write(this, resultSSLBIOWrite, bufferSSLBIOWrite, ssl_bio)) {
		result(arg, -1);
	}
}
void KSelectable::lowEventRead(void *arg,resultEvent result,bufferEvent buffer)
{
	if (buffer==NULL) {
			result(arg,0);
			return;
	}
	WSABUF recvBuf[1];
	memset(&recvBuf,0,sizeof(recvBuf));
	int bufferCount = 1;
	KSocket *server = getSocket();
	buffer(arg,recvBuf,bufferCount);
	assert(recvBuf[0].iov_len>0);
	int got = server->recv((char *)recvBuf[0].iov_base,recvBuf[0].iov_len);
	if (got>=0) {
			result(arg,got);
			return;
	}
	if (errno==EAGAIN) {
		CLR(st_flags,STF_RREADY);
		if (!selector->read(this,result,buffer,arg)) {
				result(arg,-1);
		}
		return;
	}
	result(arg,-1);
}
void KSelectable::lowEventWrite(void *arg,resultEvent result,bufferEvent buffer)
{
	if (unlikely(buffer==NULL)) {
		result(arg,0);
		return;
	}
	WSABUF recvBuf[MAXSENDBUF];
	memset(&recvBuf,0,sizeof(recvBuf));
	int bufferCount = MAXSENDBUF;
	KSocket *server = getSocket();
	buffer(arg,recvBuf,bufferCount);
	assert(recvBuf[0].iov_len>0);
	int got = server->sendev(recvBuf,bufferCount);
	if (got>=0) {
		result(arg,got);
		return;
	}
	if (errno==EAGAIN) {
		CLR(st_flags,STF_WREADY);
		if (!selector->write(this,result,buffer,arg)) {
			result(arg,-1);
		}
		return;
	}
	result(arg,-1);
}
#endif
void KSelectable::async_read(void *arg,resultEvent result,bufferEvent buffer)
{
	assert(selector->is_same_thread());
#ifdef KSOCKET_SSL
	if (isSSL()) {
		KSSLSocket *sslsocket = static_cast<KSSLSocket *>(getSocket());
		int pending_read = SSL_pending(sslsocket->getSSL());
		if (pending_read>0) {
			//ssl still have data to read
			eventRead(arg, result, buffer);
			return;
		}
#ifdef ENABLE_KSSL_BIO
		sslRead(arg, result, buffer);
		return;
#endif
	}
#endif
	if (!selector->read(this,result,buffer,arg)) {
		result(arg,-1);
	}
	return;
}
void KSelectable::eventRead(void *arg,resultEvent result,bufferEvent buffer)
{
	if (buffer==NULL) {
		result(arg,0);
		return;
	}
	WSABUF recvBuf[1];
	memset(&recvBuf,0,sizeof(recvBuf));
	int bufferCount = 1;
	KClientSocket *server = static_cast<KClientSocket *>(getSocket());
	buffer(arg,recvBuf,bufferCount);
	assert(recvBuf[0].iov_len>0);
	int got = server->read((char *)recvBuf[0].iov_base,recvBuf[0].iov_len);
	if (got>=0) {
		result(arg,got);
		return;
	}
#ifdef KSOCKET_SSL
	if (isSSL()) {
		CLR(st_flags, STF_RREADY);
		KSSLSocket *sslsocket = static_cast<KSSLSocket *>(server);
		int err = sslsocket->get_ssl_error(got);
		//printf("error=[%d]\n", err);
		if (errno == EAGAIN || err==SSL_ERROR_WANT_READ) {
#ifdef ENABLE_KSSL_BIO
			sslRead(arg, result, buffer);
			return;
#endif
			if (!selector->read(this,result,buffer,arg)) {
				result(arg,-1);
			}
			return;
		}
	}
#endif
	if (errno == EAGAIN) {
		CLR(st_flags, STF_RREADY);
		if (!selector->read(this, result, buffer, arg)) {
			result(arg, -1);
		}
		return;
	}
	result(arg,-1);
}
void KSelectable::eventWrite(void *arg,resultEvent result,bufferEvent buffer)
{
	if (unlikely(buffer==NULL)) {
		result(arg,0);
		return;
	}
	WSABUF recvBuf[MAXSENDBUF];
	memset(&recvBuf,0,sizeof(recvBuf));
	int bufferCount = MAXSENDBUF;
	KClientSocket *server = static_cast<KClientSocket *>(getSocket());
	buffer(arg,recvBuf,bufferCount);
	assert(recvBuf[0].iov_len>0);
	int got = server->writev(recvBuf,bufferCount,isSSL());
	//printf("write got=[%d]\n",got);
	if (got>=0) {
		result(arg,got);
		return;
	}
#ifdef KSOCKET_SSL
	if (isSSL()) {
		CLR(st_flags, STF_WREADY);
		KSSLSocket *sslsocket = static_cast<KSSLSocket *>(server);
		if (errno == EAGAIN || sslsocket->get_ssl_error(got)==SSL_ERROR_WANT_WRITE) {
#ifdef ENABLE_KSSL_BIO
			sslWrite(arg, result, buffer);
			return;
#endif
			if (!selector->write(this,result,buffer,arg)) {
				result(arg,-1);
			}
			return;
		}
	}
#endif
	if (errno == EAGAIN) {
		CLR(st_flags, STF_WREADY);
		if (!selector->write(this, result, buffer, arg)) {
			result(arg, -1);
		}
		return;
	}
	result(arg,-1);
}
