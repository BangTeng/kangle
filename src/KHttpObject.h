#ifndef KHTTPOBJECT_H_
#define KHTTPOBJECT_H_

#include "KMutex.h"

#include "forwin32.h"
#include "KBuffer.h"
#include "log.h"
#include "KHttpKeyValue.h"
#include "KDiskCache.h"
#include "KUrl.h"
#include "KHttpHeader.h"
#include "KHttpRequest.h"

#include "KHttpKeyValue.h"
#include "time_utils.h"
#include "KFile.h"
#include "KBuffer.h"

#define   LIST_IN_MEM   0
#define   LIST_IN_DISK  1
#define   LIST_IN_NONE  2

extern KMutex hash_lock[HASH_SIZE+1];

class KHttpObjectHash;

#define MEMORY_OBJECT       0
#define BIG_OBJECT          1
#define BIG_OBJECT_PROGRESS 2
#define SWAPING_OBJECT      3
/**
 * httpobject的信息主体
 */
class KHttpObjectBody {
public:
	KHttpObjectBody() {
		memset(this, 0, sizeof(KHttpObjectBody));
	}
	KHttpObjectBody(KHttpObjectBody *data);
	~KHttpObjectBody() {
		if (headers) {
			free_header(headers);
		}
		switch(type){
		case MEMORY_OBJECT:
			if (bodys) {
				KBuffer::destroy(bodys);
			}
			break;
		case SWAPING_OBJECT:
			if (os) {
				delete os;
			}
			break;
		
		}
	}
	unsigned short status_code;
	unsigned short type;
	int headerSize;
	KHttpHeader *headers; /* headers */
	union {
		buff *bodys; /* data storage	*/
		KHttpObjectSwaping *os;
		
	};
};
inline bool is_status_code_no_body(int status_code) {
		if (status_code == 100
			|| status_code == STATUS_NOT_MODIFIED
			|| status_code == STATUS_NO_CONTENT) {
			//no content,see rfc2616.
			return true;
		}
		return false;
}
/**
 * http物件。例如网页之类,缓存对象
 */
class KHttpObject {
public:
	KHttpObject() {
		init(NULL);
	}
	KHttpObject(KHttpRequest *rq) {		
		init(rq->url);
		data = new KHttpObjectBody();	
		SET(index.flags,FLAG_IN_MEM);
	}
	KHttpObject(KHttpObject *obj);
	void init(KUrl *url) {
		memset(&index,0,sizeof(index));
		list_state = LIST_IN_NONE;
		in_cache = 0;
		index.last_verified = kgl_current_sec;
		this->url = url;
		h = HASH_SIZE;
		refs = 1;
		data = NULL;
	}
	inline char *getCharset()
	{
		if(data==NULL){
			return NULL;
		}
		KHttpHeader *tmp = data->headers;
		while(tmp){
			if(strcasecmp(tmp->attr,"Content-Type")==0){		
				const char *p = strstr(tmp->val, "charset=");
				if (p) {
					p += 8;
					while (*p && IS_SPACE((unsigned char)*p))
						p++;
					const char *charsetend = p;
					while (*charsetend && !IS_SPACE((unsigned char)*charsetend)
							&& *charsetend != ';')
						charsetend++;
					int charset_len = charsetend - p;
					char *charset = (char *)malloc(charset_len+1);
					memcpy(charset,p,charset_len);
					charset[charset_len] = '\0';
					return charset;		
				}
				return NULL;
			}
			tmp = tmp->next;
		}
		return NULL;
	}
	KMutex *getLock()
	{
		return &hash_lock[h];
	}
	int getRefs() {
		u_short hh = h;
		hash_lock[hh].Lock();
		int ret = refs;
		hash_lock[hh].Unlock();
		return ret;
	}
	KHttpHeader *findHeader(const char *attr,int len) {
		KHttpHeader *h = data->headers;
		while (h) {
			if (is_attr(h,attr,len)) {
				return h;
			}
			h = h->next;			
		}
		return NULL;
	}
	bool matchEtag(const char *if_none_match,int len) {
		if (!TEST(index.flags,OBJ_HAS_ETAG)) {
			return false;
		}
		if (data==NULL) {
			return false;
		}
		KHttpHeader *h = findHeader("Etag",sizeof("Etag")-1);
		if (h==NULL || len!=h->val_len) {
			return false;
		}
		return memcmp(if_none_match,h->val,len)==0;
	}
	void addRef() {
		u_short hh = h;
		hash_lock[hh].Lock();
		refs++;
		hash_lock[hh].Unlock();
	}
	void release()
	{
		u_short hh = h;
		hash_lock[hh].Lock();
		assert(refs>0);
		refs--;
		if (refs==0) {
			hash_lock[hh].Unlock();
			delete this;
			return;
		}
		hash_lock[hh].Unlock();
	}
	unsigned getCurrentAge(time_t nowTime) {	
		return (unsigned) (nowTime - index.last_verified);
	}
#ifdef ENABLE_FORCE_CACHE
	//强制缓存
	void forceCache(bool insertLastModified=true)
	{
		CLR(index.flags,ANSW_NO_CACHE|OBJ_MUST_REVALIDATE);
		if (!TEST(index.flags,ANSW_LAST_MODIFIED|OBJ_HAS_ETAG)) {
			index.last_modified = kgl_current_sec;
			if (insertLastModified) {
				char *tmp_buf = (char *)malloc(41);
				memset(tmp_buf, 0, 41);
				mk1123time(index.last_modified, tmp_buf, 41);
				insertHttpHeader2(strdup("Last-Modified"),sizeof("Last-Modified")-1,tmp_buf,29);
			}
			SET(index.flags,ANSW_LAST_MODIFIED);
		}
		SET(index.flags,OBJ_IS_STATIC2);
	}
#endif
	bool isNoBody(KHttpRequest *rq) {
		if (this->checkNobody()) {
			return true;
		}
		return rq->meth == METH_HEAD;
	}
	bool checkNobody() {
		if (is_status_code_no_body(data->status_code)) {
			SET(index.flags,FLAG_NO_BODY);
			return true;
		}
		if (TEST(index.flags,ANSW_XSENDFILE)) {
			SET(index.flags,FLAG_NO_BODY);
			return true;
		}
		return false;
	}
	void unlinkDiskFile();
	bool swapin(KHttpObjectBody *data);
	bool swapinBody(KFile *fp,KHttpObjectBody *data);
	void count_size(INT64 &mem_size,INT64 &disk_size)
	{
		if (TEST(index.flags,FLAG_IN_MEM)) {
		
				mem_size += index.have_length;
		}
		if (TEST(index.flags,FLAG_IN_DISK)) {
			disk_size += index.have_length;
		}
	}
	bool swapout();
#ifdef ENABLE_DISK_CACHE
	char *getFileName(bool part=false);
#endif
	int saveHead(KFile *fp);
	int saveIndex(KFile *fp);

	bool removeHttpHeader(const char *attr)
	{
		bool result = false;
		KHttpHeader *h = data->headers;
		KHttpHeader *last = NULL;
		while (h) {
			KHttpHeader *next = h->next;
			if (strcasecmp(h->attr,attr)==0) {
				if (last) {
					last->next = next;
				} else {
					data->headers = next;
				}
				free(h->attr);
				free(h->val);
				free(h);
				h = next;
				result = true;
				continue;
			}
			last = h;
			h = next;
		}
		return result;
	}
	void insertHttpHeader2(char *attr,int attr_len,char *val,int val_len)
	{
		KHttpHeader *new_h = (KHttpHeader *) xmalloc(sizeof(KHttpHeader));
		new_h->attr = attr;
		new_h->attr_len = attr_len;
		new_h->val = val;
		new_h->val_len = val_len;
		new_h->next = data->headers;
		data->headers = new_h;
	}
	void insertHttpHeader(const char *attr,int attr_len, const char *val,int val_len) {
		insertHttpHeader2(xstrdup(attr),attr_len,xstrdup(val),val_len);
	}
	INT64 getTotalContentSize(KHttpRequest *rq)
	{
		if (TEST(index.flags,ANSW_HAS_CONTENT_RANGE)) {
			return rq->ctx->content_range_length;
		}
		return index.content_length;
	}
	KHttpObject *lnext; /* in list */
	KHttpObject *lprev; /* in list */
	KHttpObject *next;  /* in hash */	
	/* list state
	 LIST_IN_NONE
	 LIST_IN_MEM
	 LIST_IN_DISK
	 */
	unsigned char list_state;
	unsigned char in_cache;
	short h; /* hash value */
	int refs;
	KUrl *url;
	KHttpObjectBody *data;
	HttpObjectIndex index;
private:
	~KHttpObject();
};
#endif /*KHTTPOBJECT_H_*/
