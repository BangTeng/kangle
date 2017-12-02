#include "KSelectable.h"
#include "KSelector.h"
#include "KSSLSocket.h"
#include "KPipeStream.h"
#define MAXSENDBUF 16
void KSelectable::asyncWrite(void *arg, resultEvent result, bufferEvent buffer)
{
	selector->addList(this, KGL_LIST_RW);
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
bool KSelectable::sslRead(void *arg, resultEvent result, bufferEvent buffer)
{
	KSSLSocket *ssl_socket = static_cast<KSSLSocket *>(getSocket());
	KSSLBIO *ssl_bio = &ssl_socket->ssl_bio[READ_PIPE];
	ssl_bio->buffer = buffer;
	ssl_bio->result = result;
	ssl_bio->arg = arg;
	ssl_bio->st = this;
	if (BIO_pending(ssl_bio->bio) > 0) {
		eventRead(arg, result, buffer);
		return true;
	}
	if (!selector->read(this, resultSSLBIORead, bufferSSLBIORead, ssl_bio)) {
		result(arg, -1);
		return true;
	}
	return false;
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
		if (!selector->write(this,result,buffer,arg)) {
			result(arg,-1);
		}
		return;
	}
	result(arg,-1);
}
#endif
bool KSelectable::asyncRead(void *arg,resultEvent result,bufferEvent buffer,int list)
{
	selector->addList(this, list);
#ifdef KSOCKET_SSL
	if (isSSL()) {
		KSSLSocket *sslsocket = static_cast<KSSLSocket *>(getSocket());
		int pending_read = SSL_pending(sslsocket->getSSL());
		if (pending_read>0) {
			//ssl still have data to read
			eventRead(arg, result, buffer);
			return true;
		}
#ifdef ENABLE_KSSL_BIO
		return sslRead(arg, result, buffer);
#endif
	}
#endif
	if (!selector->read(this,result,buffer,arg)) {
		result(arg,-1);
		return true;
	}
	return false;
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
	if (errno==EAGAIN) {
		if (!selector->read(this,result,buffer,arg)) {
			result(arg,-1);
		}
		return;
	}
#ifdef KSOCKET_SSL
	if (isSSL()) {
		KSSLSocket *sslsocket = static_cast<KSSLSocket *>(server);
		int err = sslsocket->get_ssl_error(got);
		if (err==SSL_ERROR_WANT_READ) {
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
	if (got>=0) {
		result(arg,got);
		return;
	}
	if (errno==EAGAIN) {
		if (!selector->write(this,result,buffer,arg)) {
			result(arg,-1);
		}
		return;
	}
#ifdef KSOCKET_SSL
	if (isSSL()) {
		KSSLSocket *sslsocket = static_cast<KSSLSocket *>(server);
		if (sslsocket->get_ssl_error(got)==SSL_ERROR_WANT_WRITE) {
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
	result(arg,-1);
}
