#ifndef KSSL_BIO_H_99
#define KSSL_BIO_H_99
#include "kfeature.h"
#ifdef ENABLE_KSSL_BIO
#include "kselector.h"
#include <openssl/bio.h>
typedef struct {
	BIO *bio;
	buffer_callback buffer;
	result_callback result;
	kselectable *st;
	void *arg;
	int got;
} kssl_bio;
void kgl_bio_init_method();
BIO_METHOD *BIO_kgl_method();
kev_result result_ssl_bio_read(void *arg, int got);
int  buffer_ssl_bio_read(void *arg, LPWSABUF buf, int bufCount);
kev_result result_ssl_bio_write(void *arg, int got);
int  buffer_ssl_bio_write(void *arg, LPWSABUF buf, int bufCount);
#endif
#endif

