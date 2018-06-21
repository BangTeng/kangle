#ifndef KURL_H
#define KURL_H
#ifdef _WIN32
#pragma warning(disable:4003)
#include <direct.h>
#endif
#include "lib.h"
#include "KString.h"
#include "forwin32.h"
#include "md5.h"
#define KGL_URL_SSL       1
#define KGL_URL_IPV6      2
#define KGL_URL_VARIED    4
#define KGL_URL_REWRITED  8
#define KGL_URL_RANGED    0x10
#define KGL_URL_ENCODE    0x20
#define KGL_URL_ORIG_SSL  0x40
#define KGL_URL_BAD       0x80

#define KGL_ENCODING_DEFLATE  1
#define KGL_ENCODING_COMPRESS (1<<1)
#define KGL_ENCODING_GZIP     (1<<2)
#define KGL_ENCODING_BR       (1<<3)
#define KGL_ENCODING_UNKNOW   (1<<6)
#define KGL_ENCODING_YES      (1<<7)
class KUrl {
public:
	~KUrl() {

	}
	KUrl() {
		memset(this, 0, sizeof(KUrl));
	}
	void destroy() {
		IF_FREE(host);
		IF_FREE(path);
		IF_FREE(param);
		flag_encoding = 0;
	}
	bool match_accept_encoding(u_char accept_encoding) {
		assert(TEST(accept_encoding, KGL_ENCODING_YES) == 0);
		if (TEST(encoding, KGL_ENCODING_YES) > 0) {
			return TEST(encoding, accept_encoding) > 0;
		}
		return accept_encoding==0 || TEST(encoding,accept_encoding) == accept_encoding;
	}
	void set_content_encoding(u_char content_encoding) {
		this->encoding = (KGL_ENCODING_YES | content_encoding);
	}
	void merge_accept_encoding(u_char accept_encoding) {
		if (TEST(encoding, KGL_ENCODING_YES) > 0) {
			return;
		}
		SET(encoding, accept_encoding);
	}
	int cmpn(const KUrl *a,int n) const {
		int ret = strcasecmp(host,a->host);
		if (ret<0) {
			return -1;
		} else if (ret > 0) {
			return 1;
		}
		return strncmp(path,a->path,n);
	}
	int operator <(const KUrl &a) const {
		int ret = strcasecmp(host, a.host);
		if (ret < 0) {
			return -1;
		} else if (ret > 0) {
			return 1;
		}
		ret = strcmp(path, a.path);
		if (ret < 0) {
			return -1;
		} else if (ret > 0) {
			return 1;
		}
		if (port < a.port) {
			return -1;
		}else if(port > a.port){
			return 1;
		}
		if(param==NULL){
			if(a.param==NULL){
				return 0;
			}else{
				return 1;
			}
		}
		if(a.param==NULL){
			return -1;
		}
		return strcmp(param,a.param);
	}
	KUrl *clone() {
		KUrl *url = new KUrl;
		clone_to(url);
		return url;
	}
	//clone this to url
	void clone_to(KUrl *url) {

		url->host = xstrdup(host);
		url->path = xstrdup(path);
		if (param)
			url->param = xstrdup(param);		
		url->port = port;
		url->flags = flags;
		url->encoding = encoding;
	}
	char *getUrl() {
		KStringBuf s(128);
		if (!getUrl(s)) {
			return NULL;
		}
		return s.stealString();
	}
	char *getUrl2(int &len) {
		KStringBuf s(128);
		if (!getUrl(s)) {
			return NULL;
		}
		len = s.getSize();
		return s.stealString();
	}
	void clean_vary()
	{
		if (!TEST(flags,KGL_URL_VARIED)) {
			return;
		}
		CLR(flags,KGL_URL_VARIED);
		if (param==NULL) {
			return;
		}
		char *p = strrchr(param,VARY_URL_KEY);
		if (p) {
			*p = '\0';
		}
		if (*param=='\0') {
			xfree(param);
			param = NULL;
		}
	}
	void vary(const char *vary_key,int len)
	{
		int param_len = 0;
		if (param) {
			param_len = (int)strlen(param);
		}
		int new_len = param_len + len + 2;
		char *new_param = (char *)xmalloc(new_len);
		char *hot = new_param;
		if (param_len>0) {
			memcpy(hot,param,param_len);
			hot += param_len;
		}
		*hot = VARY_URL_KEY;
		hot++;
		memcpy(hot,vary_key,len);
		hot+=len;
		*hot = '\0';
		if (param) {
			free(param);
		}
		param = new_param;
		SET(flags,KGL_URL_VARIED);
	}
	char *getVariedOrigParam()
	{
		char *orig_param = strdup(param);
		char *p = strchr(orig_param,VARY_URL_KEY);
		if (p) {
			*p = '\0';
		}
		return orig_param;
	}
	//返回param,param_buf用于保存临时分配的内存，记得free掉
	const char *getParam(char **param_buf) {
		const char *ret = param;
		if (TEST(flags,KGL_URL_VARIED) && param) {
			char *orig_param = getVariedOrigParam();
			if (*orig_param) {
				ret = orig_param;
				*param_buf = orig_param;
			} else {
				free(orig_param);
				ret = NULL;
			}
		}
		return ret;
	}
	void getPath(KStringBuf &s, bool urlEncode = false) {
		if (urlEncode) {
			size_t len = strlen(path);
			char *newPath = url_encode(path, len, &len);
			if (newPath) {
				s.write_all(newPath, (int)len);
				free(newPath);
			}
		} else {
			s << path;
		}
		if (param && *param) {
			if (urlEncode) {
				size_t len = strlen(param);
				char *newParam = url_value_encode(param, len, &len);
				if (newParam) {
					s.write_all("?", 1);
					s.write_all(newParam, (int)len);
					free(newParam);
				}
			}
			else {
				s << "?" << param;
			}
		}
	}
	bool getUrl(KStringBuf &s,bool urlEncode=false) {
		if (host == NULL || path == NULL) {
			return false;
		}
		int defaultPort = 80;
		if(TEST(flags,KGL_URL_SSL)){
			s << "https://";
			defaultPort = 443;
		} else {
			s << "http://";
		}
		if (TEST(flags, KGL_URL_IPV6)){
			s << "[" << host << "]";
		}else{
			s << host;
		}
		if (port != defaultPort) {
			s << ":" << port;
		}
		getPath(s, urlEncode);
		return true;
	}	
	char *host;
	char *path;
	char *param;
	u_short port;
	union {
		u_short flag_encoding;
		struct {
			u_char flags;
			u_char encoding;
		};
	};
};
void free_url(KUrl *url);
#endif
