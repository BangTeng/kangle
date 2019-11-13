#ifndef KBUFFER_H_99
#define KBUFFER_H_99
#include <stdlib.h>
#include "kfeature.h"
#include "kforwin32.h"
#include "kmalloc.h"
KBEGIN_DECLS

typedef struct kbuf_s kbuf;

struct kbuf_s {
	char *data;
	kbuf *next;
	int used;
	union {
		struct {
			int skip_data_free : 1;
		};
		int flags;
	};
};
INLINE kbuf *new_pool_kbuf(kgl_pool_t *pool, int len)
{
	kbuf *b = (kbuf *)kgl_pnalloc(pool, sizeof(kbuf));
	b->used = len;
	b->flags = 0;
	b->data = (char *)kgl_pnalloc(pool, len);
	return b;
}
INLINE kbuf * new_kbuf(int len)
{
	kbuf *b = (kbuf *)xmalloc(sizeof(kbuf));
	b->used = len;
	b->flags = 0;
	b->data = (char *)xmalloc(len);
	return b;
}

INLINE void free_kbuf(kbuf *buf)
{
	if (buf->data) {
		xfree(buf->data);
	}
	xfree(buf);
}
INLINE void destroy_kbuf(kbuf *buf)
{
	kbuf *next;
	while (buf) {
		next = buf->next;
		free_kbuf(buf);
		buf = next;
	}
}
typedef struct {
	kbuf *head;
	kbuf *write_hot_buf;
	char *read_hot;
	char *write_hot;
	int total_len;
	int chunk_size;
} krw_buffer;

typedef struct {
	kbuf *head;
	char *read_hot;
	int total_len;
} kr_buffer;


typedef struct {
	char *buf;
	int buf_size;
	int used;
} ks_buffer;

void ks_buffer_init(ks_buffer *buf, int buf_size);
ks_buffer *ks_buffer_new(int chunk_size);
void ks_buffer_destroy(ks_buffer *buf);
INLINE void ks_buffer_clean(ks_buffer *buf)
{
	xfree(buf->buf);
}
void ks_write_success(ks_buffer *buf, int got);
void ks_write_str(ks_buffer *buf, const char *str, int len);
void ks_write_int(ks_buffer *buf, int val);
void ks_write_int64(ks_buffer *buf, int64_t val);
char *ks_get_write_buffer(ks_buffer *buf, int *len);
void ks_save_point(ks_buffer *buf, const char *hot, int len);

INLINE void ks_buffer_switch_read(ks_buffer *buf)
{
	buf->buf_size = buf->used;
	buf->used = 0;
}
int ks_get_read_buffers(ks_buffer *buf, LPWSABUF buffer, int buffer_count);
int ks_get_write_buffers(ks_buffer *buf, LPWSABUF buffer, int buffer_count);
//return true if still have data to read
bool ks_read_success(ks_buffer *buf, int got);
char *ks_get_read_buffer(ks_buffer *buf, int *len);

krw_buffer *krw_buffer_new(int chunk_size);
void krw_buffer_clean(krw_buffer *rw_buffer);
void krw_buffer_destroy(krw_buffer *rw_buffer);
void krw_buffer_init(krw_buffer *rw_buffer, int chunk_size);
int krw_get_read_buffers(krw_buffer *rw_buffer,LPWSABUF buffer, int buffer_count);
char *krw_get_read_buffer(krw_buffer *rw_buffer,int *len);
int krw_read(krw_buffer *rw_buffer, char *buf, int len);

//return true if still have data to read
bool krw_read_success(krw_buffer *rw_buffer,int got);
char *krw_get_write_buffer(krw_buffer *rw_buffer, int *len);
void krw_write_success(krw_buffer *rw_buffer, int got);
void krw_write_str(krw_buffer *rw_buffer,const char *buf, int len);
void krw_write_int(krw_buffer *rw_buffer, int val);
void krw_write_int64(krw_buffer *rw_buffer, int64_t val);

void krw_append(krw_buffer *rw_buffer, kbuf *buf);
void krw_insert(krw_buffer *rw_buffer, kbuf *buf);
//kr_buffer
void kr_init(kr_buffer *r_buffer, kbuf *head, int offset, int total_len, kgl_pool_t *pool);
int kr_get_read_buffers(kr_buffer *r_buffer, LPWSABUF buffer, int bc);
bool kr_read_success(kr_buffer *r_buffer, int got);
KEND_DECLS
#endif

