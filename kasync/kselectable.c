#include <assert.h>
#include <errno.h>
#include "kfeature.h"
#include "kselectable.h"
#include "ksocket.h"
#include "kmalloc.h"
#define MAXSENDBUF 16
#ifdef KSOCKET_SSL
static int kgl_ssl_writev(SSL *ssl, LPWSABUF buffer, int bc)
{
	int got = 0;
	for (int i = 0; i < bc; i++) {
		char *hot = (char *)buffer[i].iov_base;
		int len = buffer[i].iov_len;
		while (len > 0) {
#ifdef ENABLE_KSSL_BIO
			//ssl bio max write 64KB
			int left = 65536 - got;
			if (left <= 0) {
				return got;
			}
			len = MIN(left, len);
#endif
			int this_len = SSL_write(ssl, hot, len);
			//printf("SSL_write try write=[%d] return [%d] got=[%d].\n", len,this_len,got);
			if (this_len <= 0) {
				return (got > 0 ? got : this_len);
			}
			got += this_len;
			len -= this_len;
			hot += this_len;
		}
	}
	return got;
}
static int kgl_ssl_readv(SSL *ssl, LPWSABUF buffer, int bc)
{
	int got = 0;
	for (int i = 0; i < bc; i++) {
		char *hot = (char *)buffer[i].iov_base;
		int len = buffer[i].iov_len;
		while (len > 0) {
			int this_len = SSL_read(ssl, hot, len);
			if (this_len <= 0) {
				return (got > 0 ? got : this_len);
			}
			got += this_len;
			len -= this_len;
			hot += this_len;
		}
	}
	return got;
}
#endif
static int kgl_writev(SOCKET s,LPWSABUF buffer,int bc)
{
#ifdef HAVE_WRITEV
	return writev(s,buffer,bc);
#else
	int got = 0;
	for (int i=0;i<bc;i++) {
		char *hot = (char *)buffer[i].iov_base;
		int len = buffer[i].iov_len;
		while (len>0) {
			int this_len = send(s,hot,len,0);
			if (this_len<=0) {
				return (got>0?got:this_len);
			}
			got += this_len;
			len -= this_len;
			hot += this_len;
		}
	}
	return got;
#endif
}
static int kgl_readv(SOCKET s,LPWSABUF buffer,int bc)
{
#ifdef HAVE_READV
	return readv(s,buffer,bc);
#else
	int got = 0;
	for (int i=0;i<bc;i++) {
		char *hot = (char *)buffer[i].iov_base;
		int len = buffer[i].iov_len;
		while (len>0) {
			int this_len = recv(s,hot,len,0);
			if (this_len<=0) {
				return (got>0?got:this_len);
			}
			got += this_len;
			len -= this_len;
			hot += this_len;
		}
	}
	return got;
#endif

}
void selectable_clean(kselectable *st)
{
	kassert(TEST(st->st_flags, STF_LOCK) == 0);
	kassert(st->queue.next == NULL);
	kassert(st->queue.prev == NULL);
	if (ksocket_opened(st->fd)) {
		if (st->selector) {
			kgl_selector_module.remove(st->selector, st);
		}
		ksocket_shutdown(st->fd, SHUT_RDWR);
		ksocket_close(st->fd);
	}
#ifdef KSOCKET_SSL
	if (st->ssl) {
		SSL_free(st->ssl->ssl);
		xfree(st->ssl);
	}
#endif
}
bool selectable_remove(kselectable *st)
{
	kgl_selector_module.remove(st->selector, st);
	return true;
}
void selectable_shutdown(kselectable *st)
{
	if (TEST(st->st_flags, STF_RECVFROM)) {
		closesocket(st->fd);
		ksocket_init(st->fd);
		return;
	}
	ksocket_shutdown(st->fd, SHUT_RDWR);
}
void selectable_recvfrom_event(kselectable *st)
{
	WSABUF buf;
	WSABUF addr;
	int bc = st->e[OP_READ].buffer(st->e[OP_READ].arg, &buf, 1);
	kassert(bc == 1);
	bc = st->e[OP_WRITE].buffer(st->e[OP_READ].arg, &addr, 1);
	kassert(bc == 1);
	socklen_t addr_len = (socklen_t)addr.iov_len;
	int got = recvfrom(st->fd, (char *)buf.iov_base, buf.iov_len, 0, (struct sockaddr *)addr.iov_base, &addr_len);
	st->e[OP_READ].result(st->e[OP_READ].arg, got);
}
void selectable_read_event(kselectable *st)
{
	//printf("handle_read_event st=[%p]\n",st);
#ifdef STF_ET
	if (TEST(st->st_flags, STF_ET))
#endif
		CLR(st->st_flags, STF_READ);
#ifdef ENABLE_KSSL_BIO
	if (!TEST(st->st_flags, STF_RREADY2)) {
		selectable_low_event_read(st, st->e[OP_READ].result, st->e[OP_READ].buffer, st->e[OP_READ].arg);
		return;
	}
#endif
	selectable_event_read(st, st->e[OP_READ].result, st->e[OP_READ].buffer, st->e[OP_READ].arg);
}
void selectable_write_event(kselectable *st)
{
	CLR(st->st_flags, STF_WRITE|STF_RDHUP);
	if (TEST(st->st_flags, STF_ERR) > 0) {
		st->e[OP_WRITE].result(st->e[OP_WRITE].arg, -1);
		return;
	}
#ifdef ENABLE_KSSL_BIO
	if (!TEST(st->st_flags, STF_WREADY2)) {
		selectable_low_event_write(st, st->e[OP_WRITE].result, st->e[OP_WRITE].buffer, st->e[OP_WRITE].arg);
		return;
	}
#endif
	selectable_event_write(st, st->e[OP_WRITE].result, st->e[OP_WRITE].buffer, st->e[OP_WRITE].arg);
}
#ifdef ENABLE_KSSL_BIO
static bool selectable_ssl_read(kselectable *st, result_callback result, buffer_callback buffer, void *arg)
{
	kssl_bio *ssl_bio = &st->ssl->bio[OP_READ];
	ssl_bio->buffer = buffer;
	ssl_bio->result = result;
	ssl_bio->arg = arg;
	ssl_bio->st = st;
	kassert(result != result_ssl_bio_read);
	kassert(buffer != buffer_ssl_bio_read);
	kassert(arg != ssl_bio);
	if (BIO_pending(ssl_bio->bio) > 0) {
		//printf("st=[%p] bio_pending=[%d].\n", st,BIO_pending(ssl_bio->bio));
		kassert(!TEST(st->st_flags, STF_READ));
		//ssl still have data to read
		st->e[OP_READ].arg = arg;
		st->e[OP_READ].result = result;
		st->e[OP_READ].buffer = buffer;
		SET(st->st_flags, STF_READ | STF_RREADY2);
		kselector_add_list(st->selector, st, KGL_LIST_READY);
		//selectable_event_read(st,result,buffer,arg);
		return true;
	}
	return kgl_selector_module.read(st->selector, st, result_ssl_bio_read, buffer_ssl_bio_read, ssl_bio);
}
static bool selectable_ssl_write(kselectable *st, result_callback result, buffer_callback buffer, void *arg)
{
	kssl_bio *ssl_bio = &st->ssl->bio[OP_WRITE];
	ssl_bio->got = 0;
	if (buffer) {
		WSABUF recvBuf[MAXSENDBUF];
		int bufferCount = buffer(arg, recvBuf, MAXSENDBUF);
		ssl_bio->got = kgl_ssl_writev(st->ssl->ssl, recvBuf, bufferCount);
	}
	if (BIO_pending(ssl_bio->bio) <= 0) {
		st->e[OP_WRITE].arg = arg;
		st->e[OP_WRITE].result = result;
		st->e[OP_WRITE].buffer = buffer;
		SET(st->st_flags, STF_WRITE | STF_WREADY2);
		kselector_add_list(st->selector, st, KGL_LIST_READY);
		return true;
	}
	ssl_bio->buffer = buffer;
	ssl_bio->result = result;
	ssl_bio->arg = arg;
	ssl_bio->st = st;
	return kgl_selector_module.write(st->selector, st, result_ssl_bio_write, buffer_ssl_bio_write, ssl_bio);
}

void selectable_low_event_write(kselectable *st, result_callback result, buffer_callback buffer, void *arg)
{
#ifdef _WIN32
	kassert(false);
#endif
	if (unlikely(buffer == NULL)) {
		result(arg, 0);
		return;
	}
	WSABUF recvBuf[MAXSENDBUF];
	int bc = buffer(arg, recvBuf, MAXSENDBUF);
	kassert(recvBuf[0].iov_len > 0);
	int got = kgl_writev(st->fd, recvBuf, bc);
	if (got >= 0) {
		result(arg, got);
		return;
	}
	if (errno == EAGAIN) {
		CLR(st->st_flags, STF_WREADY);
		if (kgl_selector_module.write(st->selector, st, result, buffer, arg)) {
			return;
		}
	}
	result(arg, got);
}
void selectable_low_event_read(kselectable *st, result_callback result, buffer_callback buffer, void *arg)
{
#ifdef _WIN32
	kassert(false);
#endif
	if (unlikely(buffer == NULL)) {
		result(arg, 0);
		return;
	}
	WSABUF recvBuf[MAXSENDBUF];
	int bc = buffer(arg, recvBuf, MAXSENDBUF);
	kassert(recvBuf[0].iov_len > 0);
	int got = kgl_readv(st->fd, recvBuf, bc);
	if (got >= 0) {
		result(arg, got);
		return;
	}
	if (errno == EAGAIN) {
		CLR(st->st_flags, STF_RREADY);
		if (kgl_selector_module.read(st->selector, st, result, buffer, arg)) {
			return;
		}
	}
	result(arg, got);
}
#endif
bool selectable_try_write(kselectable *st, result_callback result, buffer_callback buffer, void *arg)
{
	kassert(TEST(st->st_flags, STF_WRITE|STF_WREADY2) == 0);
#ifdef ENABLE_KSSL_BIO
	if (st->ssl) {
		return selectable_ssl_write(st, result, buffer,arg);
	}
#endif
	return kgl_selector_module.write(st->selector, st, result, buffer, arg);
}
void selectable_next_read(kselectable *st, result_callback result, void *arg)
{
	kassert(TEST(st->st_flags, STF_READ|STF_RREADY2) == 0);
	kassert(kselector_is_same_thread(st->selector));
	st->e[OP_READ].arg = arg;
	st->e[OP_READ].result = result;
	st->e[OP_READ].buffer = NULL;
	SET(st->st_flags, STF_READ|STF_RREADY2);
	CLR(st->st_flags, STF_RDHUP);
	kselector_add_list(st->selector, st, KGL_LIST_READY);
}
void selectable_next_write(kselectable *st, result_callback result, void *arg)
{
	kassert(TEST(st->st_flags, STF_WRITE|STF_WREADY2) == 0);
	kassert(kselector_is_same_thread(st->selector));
	st->e[OP_WRITE].arg = arg;
	st->e[OP_WRITE].result = result;
	st->e[OP_WRITE].buffer = NULL;
	SET(st->st_flags, STF_WRITE | STF_WREADY2);
	CLR(st->st_flags, STF_RDHUP);
	kselector_add_list(st->selector, st, KGL_LIST_READY);	
}
kev_result selectable_read(kselectable *st, result_callback result, buffer_callback buffer, void *arg)
{
	if (!selectable_try_read(st, result, buffer, arg)) {
		return result(arg, -1);
	}
	return kev_ok;
}
kev_result selectable_write(kselectable *st, result_callback result, buffer_callback buffer, void *arg)
{
	if (!selectable_try_write(st, result, buffer, arg)) {
		return result(arg, -1);
	}
	return kev_ok;
}

bool selectable_try_read(kselectable *st, result_callback result, buffer_callback buffer, void *arg)
{
#ifdef KSOCKET_SSL
	if (st->ssl) {
		int pending_read = SSL_pending(st->ssl->ssl);
		if (pending_read > 0) {
			//printf("st=[%p] ssl_pending=[%d]\n",st, pending_read);
#ifdef ENABLE_KSSL_BIO
			kassert(result != result_ssl_bio_read);
#endif
			kassert(!TEST(st->st_flags, STF_READ));
			//ssl still have data to read
			st->e[OP_READ].arg = arg;
			st->e[OP_READ].result = result;
			st->e[OP_READ].buffer = buffer;
			SET(st->st_flags, STF_READ|STF_RREADY2);
			kselector_add_list(st->selector, st, KGL_LIST_READY);
			//selectable_event_read(st,result, buffer,arg);
			return true;
		}
#ifdef ENABLE_KSSL_BIO
		return selectable_ssl_read(st,result, buffer,arg);
#endif
	}
#endif
	return kgl_selector_module.read(st->selector, st, result, buffer, arg);
}

kev_result selectable_event_write(kselectable *st,result_callback result, buffer_callback buffer, void *arg)
{
	WSABUF recvBuf[MAXSENDBUF];
	if (TEST(st->st_flags, STF_WREADY2)) {
		CLR(st->st_flags, STF_WREADY2);
#ifdef ENABLE_KSSL_BIO		
		if (st->ssl && buffer) {
			kssl_bio *ssl_bio = &st->ssl->bio[OP_WRITE];			
			kassert(buffer != buffer_ssl_bio_write);
			kassert(result != result_ssl_bio_write);
			kassert(arg != ssl_bio);
			kassert(BIO_pending(ssl_bio->bio) <= 0);
			return result(arg, ssl_bio->got);
		}
#endif
	}
	if (unlikely(buffer==NULL)) {
		return result(arg,0);
	}
	int bc = buffer(arg,recvBuf, MAXSENDBUF);
	kassert(recvBuf[0].iov_len>0);
	int got;
#ifdef KSOCKET_SSL
	if (st->ssl) {
		got = kgl_ssl_writev(st->ssl->ssl, recvBuf, bc);
	} else
#endif
		got = kgl_writev(st->fd, recvBuf, bc);
	if (got>=0) {
		return result(arg,got);
	}
#ifdef KSOCKET_SSL
	if (st->ssl) {
		CLR(st->st_flags, STF_WREADY);
		int err = SSL_get_error(st->ssl->ssl, got);
		if (errno == EAGAIN || err == SSL_ERROR_WANT_WRITE) {
#ifdef ENABLE_KSSL_BIO
			if (!selectable_ssl_write(st, result, buffer, arg)) {
				return result(arg, got);
			}
			return kev_ok;
#endif
			if (!kgl_selector_module.write(st->selector, st, result, buffer, arg)) {
				return result(arg, got);
			}
			return kev_ok;
		}
	}
#endif
	if (errno==EAGAIN) {
		CLR(st->st_flags,STF_WREADY);
		if (kgl_selector_module.write(st->selector, st, result, buffer, arg)) {
			return kev_ok;
		}
	}
	return result(arg, got);
}
kev_result selectable_event_read(kselectable *st, result_callback result, buffer_callback buffer, void *arg)
{
#ifdef ENABLE_KSSL_BIO
	if (st->ssl) {
#ifndef NDEBUG
#ifdef ENABLE_KSSL_BIO
		kssl_bio *ssl_bio = &st->ssl->bio[OP_READ];
		int ssl_pending = SSL_pending(st->ssl->ssl);
		int bio_pending = BIO_pending(ssl_bio->bio);
		kassert(ssl_pending > 0 || bio_pending > 0 || ssl_bio->bio->shutdown);
#endif
#endif
		if (TEST(st->st_flags, STF_RREADY2)) {
			CLR(st->st_flags, STF_RREADY2);
			kassert(result != result_ssl_bio_read);
		}
	}
#endif
	if (unlikely(buffer==NULL)) {
		return result(arg,0);
	}
	WSABUF recvBuf[MAXSENDBUF];
	int bc = buffer(arg,recvBuf, MAXSENDBUF);
	kassert(recvBuf[0].iov_len>0);
	int got;
#ifdef KSOCKET_SSL
	if (st->ssl) {		
		got = kgl_ssl_readv(st->ssl->ssl, recvBuf, bc); 
	} else 
#endif
		got = kgl_readv(st->fd, recvBuf, bc);

	if (got>=0) {
		return result(arg,got);
	}
#ifdef KSOCKET_SSL
	if (st->ssl) {
		CLR(st->st_flags, STF_RREADY);
		int err = SSL_get_error(st->ssl->ssl, got); 
		if (errno == EAGAIN || err == SSL_ERROR_WANT_READ) {
#ifdef ENABLE_KSSL_BIO
			if (!selectable_ssl_read(st, result, buffer, arg)) {
				return result(arg, got);
			}
			return kev_ok;
#endif
			if (kgl_selector_module.read(st->selector, st, result, buffer, arg)) {
				return kev_ok;
			}
		}
		return result(arg, got);
	}
#endif
	if (errno==EAGAIN) {
		kassert(!TEST(st->st_flags, STF_RREADY2));
		CLR(st->st_flags,STF_RREADY);
		if (kgl_selector_module.read(st->selector, st, result, buffer, arg)) {
			return kev_ok;
		}
	}
	return result(arg, got);
}
int selectable_sync_read(kselectable *st, LPWSABUF buf, int bc)
{
#ifdef KSOCKET_SSL
	if (st->ssl) {
		return kgl_ssl_readv(st->ssl->ssl, buf, bc);
	}
#endif
	return kgl_readv(st->fd, buf, bc);
}
int selectable_sync_write(kselectable *st, LPWSABUF buf, int bc)
{
#ifdef KSOCKET_SSL
	if (st->ssl) {
		return kgl_ssl_writev(st->ssl->ssl, buf, bc);
	}
#endif
	return kgl_writev(st->fd, buf, bc);
}
void selectable_add_sync(kselectable *st)
{
	selectable_remove(st);
	kselector_add_list(st->selector, st, KGL_LIST_SYNC);
	int tmo = (st->tmo + 1) * st->selector->timeout[KGL_LIST_RW];
	ksocket_set_time(st->fd, tmo,tmo);
#ifndef _WIN32
	ksocket_block(st->fd);
#endif
}
void selectable_remove_sync(kselectable *st)
{
#ifndef _WIN32
	ksocket_no_block(st->fd);
#endif
	kselector_remove_list(st->selector, st);
}
bool selectable_readhup(kselectable *st, result_callback result, void *arg)
{
	return kgl_selector_module.readhup(st->selector, st, result, arg);
}
void selectable_remove_readhup(kselectable *st)
{
	kgl_selector_module.remove_readhup(st->selector, st);
}
