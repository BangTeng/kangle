#ifndef KCONTEXT_H
#define KCONTEXT_H
#include "KBuffer.h"
#include "global.h"
#include <assert.h>
#ifndef _WIN32
#include <sys/uio.h>
#endif
class KHttpTransfer;
class KHttpObject;
class KRequestQueue;
class KHttpRequest;
enum modified_type
{
	modified_if_modified,
	modified_if_range_date,
	modified_if_unmodified,
	modified_if_none_match,
	modified_if_range_etag
};
class KContext
{
public:
	inline KContext()
	{
		memset(this,0,sizeof(KContext));
	}
	~KContext()
	{
		assert(obj==NULL && old_obj==NULL);
		assert(st == NULL);
		assert(if_none_match == NULL);
	}
	void pushObj(KHttpObject *obj);
	void popObj();
	void clean_if_none_match()
	{
		if (if_none_match != NULL) {
			if (if_none_match->data) {
				free(if_none_match->data);
			}
			free(if_none_match);
			if_none_match = NULL;
		}
	}
	void set_if_none_match(const char *etag,int len)
	{
		if (if_none_match) {
			if (if_none_match->data) {
				free(if_none_match->data);
			}
		} else {
			if_none_match = (kgl_str_t *)malloc(sizeof(kgl_str_t));
		}
		if_none_match->data = (char *)malloc(len+1);
		if_none_match->len = len;
		memcpy(if_none_match->data,etag,len+1);		
	}
	KHttpObject *obj;
	KHttpObject *old_obj;
	
#ifdef ENABLE_REQUEST_QUEUE
	bool queue_handled;
#endif
	bool cache_hit;
	bool cache_hit_part;
	bool haveStored;
	bool new_object;
	bool know_length;
	bool upstream_connection_keep_alive;
	bool connection_upgrade;
	bool connection_connect_proxy;
	bool always_on_model;
	bool upstream_chunked;
	bool response_checked;
	bool no_body;
	bool upstream_sign;
	bool parent_signed;
	bool read_huped;
#ifndef NDEBUG
	bool upstream_expected_done;
	//用于调试，跟踪上流socket
	SOCKET upstream_socket;
#endif
	//lastModified类型
	modified_type mt;
	u_short us_code;
	time_t lastModified;
	kgl_str_t *if_none_match;
	INT64 content_range_length;
	int keepAlive;
	//异步读文件时需要的数据
	INT64 left_read;

	KWStream *st;
	void clean();
	void store_obj(KHttpRequest *rq);
	void clean_obj(KHttpRequest *rq,bool store_flag = true);
};

#endif
