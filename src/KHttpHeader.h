#ifndef KHTTPHEADER_H_
#define KHTTPHEADER_H_
#include "ksocket.h"
#include "ksapi.h"
#include "kmalloc.h"
#include "kstring.h"

#define MAX_HEADER_ATTR_VAL_SIZE 65500
struct kgl_keyval_t {
	kgl_str_t   key;
	kgl_str_t   value;
};
enum know_http_header
{
	kgl_response_server,
	kgl_response_date
};
extern kgl_str_t know_http_headers[];
void *kgl_memstr(char *haystack, int haystacklen, char *needle, int needlen);
#define kgl_cpymem(dst, src, n)   (((u_char *) kgl_memcpy(dst, src, n)) + (n))
inline KHttpHeader *new_http_header(kgl_pool_t *pool,const char *attr, int attr_len, const char *val, int val_len) {
	if (attr_len > MAX_HEADER_ATTR_VAL_SIZE || val_len > MAX_HEADER_ATTR_VAL_SIZE) {
		return NULL;
	}
	KHttpHeader *header = (KHttpHeader *)kgl_pnalloc(pool,sizeof(KHttpHeader));
	header->next = NULL;
	header->attr = (char *)kgl_pnalloc(pool,attr_len + 1);
	kgl_memcpy(header->attr, attr, attr_len);
	header->attr[attr_len] = '\0';
	header->attr_len = attr_len;
	header->val = (char *)kgl_pnalloc(pool, val_len + 1);
	kgl_memcpy(header->val, val, val_len);
	header->val[val_len] = '\0';
	header->val_len = val_len;
	return header;
}
inline KHttpHeader *new_http_header(const char *attr,int attr_len,const char *val,int val_len) {
	if (attr_len > MAX_HEADER_ATTR_VAL_SIZE || val_len>MAX_HEADER_ATTR_VAL_SIZE) {
		return NULL;
	}
	KHttpHeader *header = (KHttpHeader *)malloc(sizeof(KHttpHeader));
	header->next = NULL;
	header->attr = (char *)malloc(attr_len+1);
	kgl_memcpy(header->attr,attr,attr_len);
	header->attr[attr_len] = '\0';
	header->attr_len = attr_len;
	header->val = (char *)malloc(val_len+1);
	kgl_memcpy(header->val,val,val_len);
	header->val[val_len] = '\0';
	header->val_len = val_len;
	return header;
}
inline void free_header(struct KHttpHeader *av) {
	struct KHttpHeader *next;
	while (av) {
		next = av->next;
		free(av->attr);
		free(av->val);
		free(av);
		av = next;
	}
}
bool is_attr(KHttpHeader *av, const char *attr,int attr_len);
inline char *strlendup(const char *str, int len)
{
	char *buf = (char *)malloc(len + 1);
	kgl_memcpy(buf, str, len);
	buf[len] = '\0';
	return buf;
}
class KHttpHeaderManager {
public:
	void Append(KHttpHeader *new_t)
	{
		if (header == NULL) {
			header = last = new_t;
			return;
		}
		new_t->next = header;
		header = new_t;
	}
	void Insert(KHttpHeader *new_t)
	{
		if (header == NULL) {
			header = last = new_t;
			return;
		}
		kassert(last);
		last->next = new_t;
		last = new_t;
		return;
	}
	bool AddHeader(const char *attr, int attr_len, const char *val, int val_len, bool tail = true)
	{
		if (attr_len > MAX_HEADER_ATTR_VAL_SIZE || val_len > MAX_HEADER_ATTR_VAL_SIZE) {
			return false;
		}
		KHttpHeader *new_t = new KHttpHeader;
		if (new_t == NULL) {
			return false;
		}
		new_t->attr = strlendup(attr, attr_len);
		new_t->attr_len = attr_len;
		new_t->val = strlendup(val, val_len);
		new_t->val_len = val_len;
		new_t->next = NULL;
		if (tail) {
			Insert(new_t);
			return true;
		}
		Append(new_t);
		return true;
	}
	KHttpHeader *FindHeader(const char *attr, int len)
	{
		KHttpHeader *l = header;
		while (l) {
			if (is_attr(l,attr,len)) {
				return l;
			}		
			l = l->next;
		}
		return NULL;
	}
	KHttpHeader *RemoveHeader(const char *attr)
	{
		KHttpHeader *l = header;
		KHttpHeader *prev = NULL;
		while (l) {
			if (strcasecmp(l->attr, attr) == 0) {
				if (prev) {
					prev->next = l->next;
				} else {
					header = l->next;
				}
				return l;
			}
			prev = l;
			l = l->next;
		}
		return NULL;
	}
	KHttpHeader *GetHeader()
	{
		return header;
	}
	KHttpHeader *StealHeader()
	{
		KHttpHeader *h = header;
		header = last = NULL;
		return h;
	}
	KHttpHeader *header;
	KHttpHeader *last;
};
#endif /*KHTTPHEADER_H_*/
