#include <errno.h>
#include "kssl_bio.h"
#include "kbuf.h"
#include "kselectable.h"
#include "klog.h"
#ifdef ENABLE_KSSL_BIO
static int kgl_bio_write(BIO *h, const char *buf, int num)
{
	if (buf == NULL) {
		klog(KLOG_ERR, "ssl_bio write data is NULL,len=[%d]\n", num);
		return -1;
	}
	krw_buffer *bb = (krw_buffer *)h->ptr;
	krw_write_str(bb, buf, num);
	return num;
}
static int kgl_bio_read(BIO *h, char *buf, int size)
{
	krw_buffer *bb = (krw_buffer *)h->ptr;
	if (buf==NULL) {
		klog(KLOG_ERR,"ssl_bio read buf is NULL[%d]\n");
		return -1;
	}
	int got = krw_read(bb, buf, size);
	//printf("bio_read bio=[%p] size=[%d] got=[%d] buf_len=[%d] shutdown=[%d]\n", h, size,got,bb->buf.getLen(),h->shutdown);
	BIO_clear_retry_flags(h);
	if (got <= 0) {
		if (!h->shutdown) {
			BIO_set_retry_read(h);
			errno = EAGAIN;
			//_set_errno(EAGAIN);
			return -1;
		}
	}
	return got;
}
static int kgl_bio_puts(BIO *h, const char *str)
{
	int n = (int)strlen(str);
	return kgl_bio_write(h, str, n);
}
static long kgl_bio_ctrl(BIO *h, int cmd, long arg1, void *arg2)
{
	krw_buffer *bb = (krw_buffer *)h->ptr;
	//long ret;
	//printf("kgl_bio_ctrl cmd=[%d] arg1=[%d]\n", cmd, arg1);
	switch (cmd) {
	case BIO_CTRL_PENDING:
		return (long)bb->total_len;
	case BIO_CTRL_WPENDING:
		return 0;
	case BIO_CTRL_GET_CLOSE:
		return h->shutdown;
	case BIO_CTRL_SET_CLOSE:
		h->shutdown = (int)arg1;
		return 1;
	case BIO_CTRL_DUP:
	case BIO_CTRL_FLUSH:
		return 1;
	case BIO_CTRL_PUSH:
	case BIO_CTRL_POP:
	default:
		break;
	}
	return 0;
}
static int kgl_bio_new(BIO *bi)
{
	krw_buffer *bb = krw_buffer_new(16384);
	if (bb == NULL) {
		return 0;
	}
	bi->shutdown = 0;
	bi->init = 1;
	bi->num = 0;
	bi->flags = 0;
	bi->ptr = (char *)bb;
	return 1;
}
static int kgl_bio_buf_free(BIO *a, int free_all)
{
	if (a == NULL || !a->init) {
		return (0);
	}
	krw_buffer *bb = (krw_buffer *)a->ptr;
	if (bb != NULL) {
		krw_buffer_destroy(bb);
	}
	a->ptr = NULL;
	return (1);
}
static int kgl_bio_free(BIO *h)
{
	return kgl_bio_buf_free(h, 1);
}
static BIO_METHOD kgl_method = {
	BIO_TYPE_MEM,
	"kgl buffer",
	kgl_bio_write,
	kgl_bio_read,
	kgl_bio_puts,
	NULL,
	kgl_bio_ctrl,
	kgl_bio_new,
	kgl_bio_free,
	NULL,
};
BIO_METHOD *BIO_kgl_method()
{
	return &kgl_method;
}

kev_result result_ssl_bio_read(void *arg, int got)
{
	kssl_bio *ssl_bio = (kssl_bio *)arg;
	krw_buffer *bb = (krw_buffer *)ssl_bio->bio->ptr;
	if (got == ST_ERR_TIME_OUT) {
		return ssl_bio->result(ssl_bio->arg, got);
	}
	if (got <= 0) {
		ssl_bio->bio->shutdown = 1;
	} else {
		krw_write_success(bb, got);
	}
	return selectable_event_read(ssl_bio->st, ssl_bio->result, ssl_bio->buffer, ssl_bio->arg);
}
int  buffer_ssl_bio_read(void *arg, LPWSABUF buf, int bufCount)
{
	kssl_bio *ssl_bio = (kssl_bio *)arg;
	krw_buffer *bb = (krw_buffer *)ssl_bio->bio->ptr;
	int len;
	buf[0].iov_base = krw_get_write_buffer(bb,&len);
	buf[0].iov_len = len;
	return 1;
}
kev_result result_ssl_bio_write(void *arg, int got)
{
	kssl_bio *ssl_bio = (kssl_bio *)arg;
	krw_buffer *bb = (krw_buffer *)ssl_bio->bio->ptr;
	if (got <= 0) {
		return ssl_bio->result(ssl_bio->arg, got);
	}	
	if (krw_read_success(bb, got)) {
		if (!kgl_selector_module.write(ssl_bio->st->selector, ssl_bio->st, result_ssl_bio_write, buffer_ssl_bio_write, ssl_bio)) {
			return ssl_bio->result(ssl_bio->arg, -1);
		}
		return kev_ok;
	}
	return ssl_bio->result(ssl_bio->arg, ssl_bio->got);
}
int buffer_ssl_bio_write(void *arg, LPWSABUF buf, int bufCount)
{
	kssl_bio *ssl_bio = (kssl_bio *)arg;
	krw_buffer *bb = (krw_buffer *)ssl_bio->bio->ptr;
	return krw_get_read_buffers(bb, buf, bufCount);
}

#endif
