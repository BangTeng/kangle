#ifndef KSTRING_H_99
#define KSTRING_H_99
#include <stdlib.h>
#include "kfeature.h"
#include "kforwin32.h"
KBEGIN_DECLS
typedef struct {
	char *data;
	size_t len;
} kgl_str_t;

#define kgl_expand_string(str)  (char *)str ,sizeof(str) - 1
#define kgl_string(str)     { (char *)str,sizeof(str) - 1 }
#define kgl_null_string     {  NULL,0 }
#define kgl_str_set(str, text) \
    (str)->len = sizeof(text) - 1; (str)->data = (char *) text
#define kgl_str_null(str)   (str)->len = 0; (str)->data = NULL

INLINE int64_t string2int(const char *buf) {
#ifdef _WIN32
	return _atoi64(buf);
#else
	return atoll(buf);
#endif
}

KEND_DECLS
#endif
