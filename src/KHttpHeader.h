#ifndef KHTTPHEADER_H_
#define KHTTPHEADER_H_
#include "KSocket.h"
#include "ksapi.h"
#define MAX_HEADER_ATTR_VAL_SIZE 65500
struct kgl_str_t
{
	char *data;
	size_t len;
};
struct kgl_keyval_t {
	kgl_str_t   key;
	kgl_str_t   value;
};
#define kgl_expand_string(str)  (char *)str ,sizeof(str) - 1
#define kgl_string(str)     { (char *)str,sizeof(str) - 1 }
#define kgl_null_string     {  NULL,0 }
#define kgl_str_set(str, text) \
    (str)->len = sizeof(text) - 1; (str)->data = (char *) text
#define kgl_str_null(str)   (str)->len = 0; (str)->data = NULL
enum know_http_header
{
	kgl_response_server,
	kgl_response_date
};
extern kgl_str_t know_http_headers[];
void *kgl_memstr(char *haystack, int haystacklen, char *needle, int needlen);
#define kgl_cpymem(dst, src, n)   (((u_char *) memcpy(dst, src, n)) + (n))
inline KHttpHeader *new_http_header(const char *attr,int attr_len,const char *val,int val_len) {
	if (attr_len > MAX_HEADER_ATTR_VAL_SIZE || val_len>MAX_HEADER_ATTR_VAL_SIZE) {
		return NULL;
	}
	KHttpHeader *header = (KHttpHeader *)malloc(sizeof(KHttpHeader));
	header->next = NULL;
	header->attr = (char *)malloc(attr_len+1);
	memcpy(header->attr,attr,attr_len);
	header->attr[attr_len] = '\0';
	header->attr_len = attr_len;
	header->val = (char *)malloc(val_len+1);
	memcpy(header->val,val,val_len);
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
#endif /*KHTTPHEADER_H_*/
