#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "kbuf.h"
#include "kmalloc.h"
#include "kforwin32.h"
void krw_append(krw_buffer *rw_buffer, kbuf *buf)
{
	if (rw_buffer->write_hot_buf == NULL) {
		kassert(rw_buffer->head == NULL && rw_buffer->read_hot == NULL);
		rw_buffer->head = buf;
		rw_buffer->read_hot = buf->data;
	} else {
		kassert(rw_buffer->read_hot && rw_buffer->head);
		rw_buffer->write_hot_buf->next = buf;
	}
	buf->next = NULL;
	rw_buffer->write_hot_buf = buf;
	rw_buffer->total_len += buf->used;
	rw_buffer->write_hot = NULL;
}
void krw_insert(krw_buffer *rw_buffer, kbuf *buf)
{
	if (rw_buffer->head == NULL) {
		kassert(rw_buffer->write_hot_buf == NULL);
		krw_append(rw_buffer, buf);
		return;
	}
	buf->next = rw_buffer->head;
	rw_buffer->head = buf;
	rw_buffer->total_len += buf->used;
	rw_buffer->read_hot = rw_buffer->head->data;
	kassert(rw_buffer->write_hot_buf);
}
void krw_buffer_clean(krw_buffer *rw_buffer)
{
	while (rw_buffer->head) {
		kbuf *next = rw_buffer->head->next;
		xfree(rw_buffer->head->data);
		xfree(rw_buffer->head);
		rw_buffer->head = next;
	}
}
void krw_buffer_destroy(krw_buffer *rw_buffer)
{
	krw_buffer_clean(rw_buffer);
	xfree(rw_buffer);
}
krw_buffer *krw_buffer_new(int chunk_size)
{
	krw_buffer *rw_buffer = (krw_buffer *)xmalloc(sizeof(krw_buffer));
	krw_buffer_init(rw_buffer,chunk_size);
	return rw_buffer;
}
void krw_buffer_init(krw_buffer *rw_buffer, int chunk_size)
{
	memset(rw_buffer, 0, sizeof(krw_buffer));
	rw_buffer->chunk_size = chunk_size;
}
void ks_buffer_init(ks_buffer *buf, int buf_size)
{
	memset(buf, 0, sizeof(ks_buffer));
	buf->buf = (char *)xmalloc(buf_size);
	buf->buf_size = buf_size;
}
ks_buffer *ks_buffer_new(int chunk_size)
{
	ks_buffer *b = (ks_buffer *)xmalloc(sizeof(ks_buffer));
	ks_buffer_init(b, chunk_size);
	return b;
}
void ks_buffer_destroy(ks_buffer *buf)
{
	ks_buffer_clean(buf);
	xfree(buf);
}
void ks_save_point(ks_buffer *buf, const char *hot, int len)
{
	kassert(buf->buf_size > 0);
	if (len == buf->used && buf->used == buf->buf_size) {
		kassert(buf->buf == hot);
		int new_size = buf->buf_size * 2;
		char *nb = (char *)xmalloc(new_size);
		kgl_memcpy(nb, buf->buf, len);
		xfree(buf->buf);
		buf->buf = nb;
		buf->buf_size = new_size;
	} else if (len > 0) {
		kassert(hot - buf->buf + len == buf->used);
		memmove(buf->buf, hot, len);
	}
	buf->used = len;
}
kbuf *krw_newbuff(int chunk_size)
{
	kbuf *buf = (kbuf *)xmalloc(sizeof(kbuf));
	buf->flags = 0;
	buf->data = (char *)malloc(chunk_size);
	buf->used = 0;
	buf->next = NULL;
	return buf;
}
int krw_get_read_buffers(krw_buffer *rw_buffer, LPWSABUF buffer, int buffer_count)
{
	if (rw_buffer->read_hot == NULL) {
		return 0;
	}
	kassert(rw_buffer->head);
	kbuf *tmp = rw_buffer->head;
	buffer[0].iov_base = rw_buffer->read_hot;
	buffer[0].iov_len = (int)(rw_buffer->head->used - (rw_buffer->read_hot - rw_buffer->head->data));
	int i;
	for (i = 1; i < buffer_count; i++) {
		tmp = tmp->next;
		if (tmp == NULL) {
			break;
		}
		buffer[i].iov_base = tmp->data;
		buffer[i].iov_len = tmp->used;
	}
	return i;
}
char *krw_get_read_buffer(krw_buffer *rw_buffer, int *len)
{
	WSABUF buffer;
	int bufferCount = krw_get_read_buffers(rw_buffer,&buffer, 1);
	if (bufferCount == 0) {
		return NULL;
	}
	*len = (int)buffer.iov_len;
	return (char *)buffer.iov_base;
}
bool krw_read_success(krw_buffer *rw_buffer, int got)
{
	kassert(rw_buffer->read_hot && rw_buffer->head);
	while (got > 0) {
		int hot_left = (int)(rw_buffer->head->used - (rw_buffer->read_hot - rw_buffer->head->data));
		int this_len = MIN(got, hot_left);
		rw_buffer->read_hot += this_len;
		got -= this_len;
		rw_buffer->total_len -= this_len;
		if (rw_buffer->head->used == rw_buffer->read_hot - rw_buffer->head->data) {
			kbuf *next = rw_buffer->head->next;
			xfree(rw_buffer->head->data);
			xfree(rw_buffer->head);
			rw_buffer->head = next;
			if (rw_buffer->head == NULL) {
				kassert(rw_buffer->total_len == 0);
				krw_buffer_init(rw_buffer,rw_buffer->chunk_size);
				return false;
			}
			rw_buffer->read_hot = rw_buffer->head->data;
		}
	}
	return true;
}
char *krw_get_write_buffer(krw_buffer *rw_buffer, int *len)
{
	if (rw_buffer->write_hot_buf == NULL) {
		kassert(rw_buffer->head == NULL);
		rw_buffer->head = krw_newbuff(rw_buffer->chunk_size);
		rw_buffer->read_hot = rw_buffer->head->data;
		rw_buffer->write_hot_buf = rw_buffer->head;
		rw_buffer->write_hot = rw_buffer->head->data;
	}
	*len = rw_buffer->chunk_size - rw_buffer->write_hot_buf->used;
	if (*len == 0 || rw_buffer->write_hot==NULL) {
		kbuf *nbuf = krw_newbuff(rw_buffer->chunk_size);
		kassert(rw_buffer->write_hot_buf->next == NULL);
		rw_buffer->write_hot_buf->next = nbuf;
		rw_buffer->write_hot_buf = nbuf;
		rw_buffer->write_hot = rw_buffer->write_hot_buf->data;
		*len = rw_buffer->chunk_size;
	}
	kassert(*len > 0);
	return rw_buffer->write_hot;
}
void krw_write_success(krw_buffer *rw_buffer, int got)
{
	kassert(rw_buffer->write_hot_buf != NULL);
	rw_buffer->write_hot_buf->used += got;
	rw_buffer->total_len += got;
	rw_buffer->write_hot += got;
}
void krw_write_str(krw_buffer *rw_buffer, const char *buf, int len)
{
	while (len > 0) {
		int wlen;
		char *t = krw_get_write_buffer(rw_buffer,&wlen);
		kassert(t);
		wlen = MIN(len, wlen);
		kgl_memcpy(t, buf, wlen);
		buf += wlen;
		len -= wlen;
		krw_write_success(rw_buffer,wlen);
	}
}
void krw_write_int(krw_buffer *rw_buffer, int val)
{
	char buf[16];
	memset(buf, 0, sizeof(buf));
	int len = snprintf(buf, sizeof(buf), "%d", val);
	krw_write_str(rw_buffer, buf, len);
}
void krw_write_int64(krw_buffer *rw_buffer, int64_t val)
{
	char buf[32];
	memset(buf, 0, sizeof(buf));
	int len = snprintf(buf, sizeof(buf), "%lld", (INT64)val);
	krw_write_str(rw_buffer, buf, len);
}
int krw_read(krw_buffer *rw_buffer, char *buf, int len)
{
	char *hot = buf;
	int got = 0;
	while (len > 0) {
		int length;
		char *read_data = krw_get_read_buffer(rw_buffer,&length);
		if (read_data == NULL) {
			return 0;
		}
		length = MIN(length, len);
		if (length <= 0) {
			break;
		}
		kgl_memcpy(hot, read_data, length);
		hot += length;
		len -= length;
		got += length;		
		if (!krw_read_success(rw_buffer, length)) {
			break;
		}
	}
	return got;
}
void ks_write_success(ks_buffer *buf, int got)
{
	buf->used += got;
}
void ks_write_str(ks_buffer *buf, const char *str, int len)
{
	while (len > 0) {
		int wlen;
		char *t = ks_get_write_buffer(buf, &wlen);
		kassert(t);
		wlen = MIN(len, wlen);
		kgl_memcpy(t, str, wlen);
		str += wlen;
		len -= wlen;
		ks_write_success(buf, wlen);
	}
}
void ks_write_int(ks_buffer *sbuf, int val)
{
	char buf[16];
	memset(buf, 0, sizeof(buf));
	int len = snprintf(buf, sizeof(buf), "%d", val);
	ks_write_str(sbuf, buf, len);
}
void ks_write_int64(ks_buffer *buf, int64_t val)
{
	char tbuf[32];
	memset(tbuf, 0, sizeof(tbuf));
	int len = snprintf(tbuf, sizeof(tbuf), "%lld", (INT64)val);
	ks_write_str(buf, tbuf, len);
}
char *ks_get_write_buffer(ks_buffer *buf, int *len)
{
	kassert(buf->buf_size > 0);
	for (;;) {
		int left = buf->buf_size - buf->used;
		if (left <= 0) {
			int new_size = buf->buf_size * 2;
			new_size = kgl_align(new_size, 1024);
			buf->buf_size = new_size;
			char *n = (char *)xmalloc(buf->buf_size);
			kgl_memcpy(n, buf->buf, buf->used);
			xfree(buf->buf);
			buf->buf = n;
			continue;
		}
		*len = left;
		return buf->buf + buf->used;
	}
}
char *ks_get_read_buffer(ks_buffer *buf, int *len)
{
	WSABUF buffer;
	int bufferCount = ks_get_read_buffers(buf, &buffer, 1);
	if (bufferCount == 0) {
		return NULL;
	}
	*len = (int)buffer.iov_len;
	return (char *)buffer.iov_base;
}
bool ks_read_success(ks_buffer *buf, int got)
{
	buf->used += got;
	kassert(buf->buf_size >= buf->used);
	return buf->buf_size > buf->used;
}
int ks_get_read_buffers(ks_buffer *buf, LPWSABUF buffer, int buffer_count)
{
	buffer[0].iov_base = buf->buf + buf->used;
	buffer[0].iov_len = buf->buf_size - buf->used;
	return 1;
}
int ks_get_write_buffers(ks_buffer *buf, LPWSABUF buffer, int buffer_count)
{
	int len;
	buffer[0].iov_base = ks_get_write_buffer(buf, &len);
	buffer[0].iov_len = len;
	return 1;
}

void kr_init(kr_buffer *r_buffer, kbuf *buf, int start, int total_len, kgl_pool_t *pool)
{
	kassert(r_buffer->head == NULL);
	r_buffer->total_len = total_len;
	while (buf) {
		if (start < buf->used) {
			if (start == 0) {
				kassert(buf->skip_data_free == 0);
				r_buffer->head = buf;
			} else {
				kbuf *t = (kbuf *)kgl_pnalloc(pool, sizeof(kbuf));
				memset(t, 0, sizeof(kbuf));
				t->data = buf->data + start;
				t->used = buf->used - start;
				t->next = buf->next;
				r_buffer->head = t;
			}
			break;
		}
		start -= buf->used;
		buf = buf->next;
	}
	if (r_buffer->head) {
		r_buffer->read_hot = r_buffer->head->data;
	}
}
int kr_get_read_buffers(kr_buffer *r_buffer, LPWSABUF buffer, int bc)
{
	kassert(r_buffer->read_hot);
	int got = r_buffer->total_len;
	kassert(r_buffer->head);
	kbuf *tmp = r_buffer->head;
	buffer[0].iov_base = r_buffer->read_hot;
	int hot_left = r_buffer->head->used - (int)(r_buffer->read_hot - r_buffer->head->data);
	hot_left = MIN(hot_left, got);
	buffer[0].iov_len = hot_left;
	got -= hot_left;
	int i;
	for (i = 1; i < bc; i++) {
		tmp = tmp->next;
		if (tmp == NULL || got <= 0) {
			break;
		}
		buffer[i].iov_base = tmp->data;
		buffer[i].iov_len = MIN(got, tmp->used);
		got -= buffer[i].iov_len;
	}
	return i;
}
bool kr_read_success(kr_buffer *r_buffer, int got)
{
	r_buffer->total_len -= got;
	while (got > 0) {
		int hot_left = r_buffer->head->used - (int)(r_buffer->read_hot - r_buffer->head->data);
		int this_len = MIN(got, hot_left);
		r_buffer->read_hot += this_len;
		got -= this_len;
		if (r_buffer->head->used == r_buffer->read_hot - r_buffer->head->data) {
			r_buffer->head = r_buffer->head->next;
			if (r_buffer->head == NULL) {
				kassert(r_buffer->total_len == 0);
				return false;
			}
			r_buffer->read_hot = r_buffer->head->data;
		}
	}
	return r_buffer->total_len>0;
}
