/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */
#ifndef request_h_include
#define request_h_include
#include <string.h>
#include <map>
#include <list>
#include <string>
#include "global.h"
#include "ksocket.h"
#include "KMutex.h"
#include "KAcserver.h"
#include "KHttpHeader.h"
#include "KBuffer.h"
#include "KAutoBuffer.h"
#include "KReadWriteBuffer.h"
#include "KDomainUser.h"
#include "KSendable.h"
#include "KStringBuf.h"
#include "KHttpAuth.h"
#include "do_config.h"
#include "KContext.h"
#include "KResponseContext.h"
#include "KUrl.h"
#include "KFileName.h"
#include "KSpeedLimit.h"
#include "KTempFile.h"
#include "KInputFilter.h"
#include "kserver.h"
#include "KFlowInfo.h"
#include "KHttp2.h"
#include "kmalloc.h"
#include "kconnection.h"
#include "KHttpParser.h"
#include "KSink.h"
#define KGL_REQUEST_POOL_SIZE 4096
#ifdef ENABLE_STAT_STUB
extern volatile uint64_t kgl_total_requests ;
extern volatile uint64_t kgl_total_accepts;
extern volatile uint64_t kgl_total_servers;
extern volatile uint32_t kgl_reading;
extern volatile uint32_t kgl_writing;
extern volatile uint32_t kgl_waiting;
#endif
#define READ_BUFF_SZ	8192

class KFetchObject;
class KSubVirtualHost;
class KFilterHelper;
class KHttpObject;
class KAccess;
class KFilterKey;
class KRequestQueue;
class KBigObjectContext;
class KSink;
class KHttpTransfer;
#define		REQUEST_EMPTY	0
#define		REQUEST_READY	1
#define 	MIN_SLEEP_TIME	4
#define FOLLOW_LINK_ALL  1
#define FOLLOW_LINK_OWN  2
#define FOLLOW_PATH_INFO 4


class KManageIP {
public:
	KMutex ip_lock;
	std::map<ip_addr, unsigned> ip_map;
};

class KContext;
class KOutputFilterContext;
class KHttpFilterContext;
class KHttpRequest;
typedef kev_result(*KHttpRequestWriteHook)(KHttpRequest *rq);

struct kgl_write_hook {
	void *arg;
	KHttpRequestWriteHook call;
	kgl_write_hook *next;
};
typedef struct {
	kgl_app_event ev;
	KHttpRequest *rq;
} kgl_request_event_context;

typedef struct {
	void *arg;
	int got;
	result_callback result;
	kgl_write_hook *hook_head;
	kgl_write_hook *hook_last;
} kgl_request_stack;
kev_result kgl_call_write_hook(void *arg, int got);
class KHttpRequestData
{
public:
	uint32_t flags;
	uint32_t filter_flags;
	//post���ݻ�ʣ��������û����
	INT64 left_read;
	//post���ݳ���
	INT64 content_length;
	time_t if_modified_since;
	time_t min_obj_verified;
	INT64 range_from;
	INT64 range_to;
	uint16_t status_code;
	uint16_t self_port;
	//unsigned short cookie_stick;
	INT64 send_size;
	INT64 begin_time_msec;
	INT64 first_response_time_msec;
	//�������ݺ��hook
	kgl_request_stack *write_stack;
};
class KHttpRequest : public KHttpRequestData,public KHttpHeaderManager {
public:
	inline KHttpRequest(KSink *sink, kgl_pool_t *pool)
	{
		memset(this, 0, sizeof(*this));
		InitPool(pool);
		ctx = new KContext;
		this->sink = sink;
		begin_time_msec = kgl_current_msec;
	}
	~KHttpRequest();	
	void clean();
	void init(kgl_pool_t *pool);
	bool isBad();
	void set_url_param(char *param);
	//�ж��Ƿ���post���ݿɶ�
	bool has_post_data();

	std::string getInfo();
	char *getUrl();
	void beginRequest();
	void endRequest();
	int getFollowLink()
	{
		int follow_link = 0;
		if (conf.path_info) {
			follow_link|=FOLLOW_PATH_INFO;
		}
		if (TEST(filter_flags,RF_FOLLOWLINK_OWN)) {
			follow_link|=FOLLOW_LINK_OWN;
			return follow_link;
		}
		if (TEST(filter_flags,RF_FOLLOWLINK_ALL)) {
			follow_link|=FOLLOW_LINK_ALL;
		}
		return follow_link;
	}
	void startParse();
	void closeFetchObject(bool destroy=true);
	void resetFetchObject();
	void FreeLazyMemory();
	bool rewriteUrl(const char *newUrl, int errorCode = 0,const char *prefix = NULL);
	const char *getState();
	void setState(uint8_t state);

	uint8_t http_major : 4;
	uint8_t http_minor : 4;
	uint8_t state;
	uint8_t meth;
	uint8_t mark;

	KSink *sink;
#ifdef ENABLE_TF_EXCHANGE
	friend class KTempFile;
	//��ʱ�ļ�
	KTempFile *tf;
	void closeTempFile()
	{
		if (tf) {
			delete tf;
			tf = NULL;
		}
		SET(flags,RQ_TEMPFILE_HANDLED);
	}
#endif
	//����Դ
	KFetchObject *fetchObj;
	//�����ļ�ӳ��
	KFileName *file;
	//��������
	KSubVirtualHost *svh;
	void releaseVirtualHost();
	/*
	 * ԭʼurl
	 */
	KUrl raw_url;
	KUrl *url;
	KHttpTransfer *tr;
	//http��֤
	KHttpAuth *auth;
	//�й�object������������
	KContext *ctx;	
	//����������
#ifdef ENABLE_INPUT_FILTER
	bool hasInputFilter()
	{
		if (if_ctx==NULL) {
			return false;
		}
		return !if_ctx->isEmpty();
	}
	/************
	* �������
	*************/
	KInputFilterContext *if_ctx;
	KInputFilterContext *getInputFilterContext()
	{
		if (if_ctx == NULL && (content_length>0 || url->param)) {
			if_ctx = new KInputFilterContext(this);
		}
		return if_ctx;
	}
#endif
	/****************
	* �������
	*****************/
	KOutputFilterContext *of_ctx;
	KOutputFilterContext *getOutputFilterContext();
	void addFilter(KFilterHelper *chain);
	inline bool responseStatus(uint16_t status_code)
	{
		if (this->status_code > 0) {
			//status_codeֻ�ܷ���һ��
			return false;
		}
		first_response_time_msec = kgl_current_msec;
		setState(STATE_SEND);
		this->status_code = status_code;
		return sink->ResponseStatus(status_code);
	}
	inline bool responseHeader(know_http_header name,const char *val,hlen_t val_len)
	{
		return this->responseHeader(know_http_headers[name].data, (hlen_t)know_http_headers[name].len,val,val_len);
	}
	inline bool responseHeader(KHttpHeader *header)
	{
		return responseHeader(header->attr,header->attr_len,header->val,header->val_len);
	}
	inline bool responseHeader(kgl_str_t *name,kgl_str_t *val)
	{
		return responseHeader(name->data, hlen_t(name->len),val->data,hlen_t(val->len));
	}
	inline bool responseHeader(kgl_str_t *name,const char *val,hlen_t val_len)
	{
		return responseHeader(name->data, hlen_t(name->len),val, hlen_t(val_len));
	}
	inline bool responseHeader(const char *name,hlen_t name_len,int val)
	{
		char buf[16];
		int len = snprintf(buf,sizeof(buf)-1,"%d",val);
		return responseHeader(name,name_len,buf,len);
	}
	//����true��һ����Ҫ��Ӧcontent-length��chunk
	inline bool responseConnection() {
#ifdef HTTP_PROXY
		if (ctx->connection_connect_proxy) {
			return false;
		}
#endif
		if (ctx->connection_upgrade) {
			return sink->ResponseConnection(kgl_expand_string("upgrade"));
		} else if (TEST(flags, RQ_CONNECTION_CLOSE) || !TEST(flags, RQ_HAS_KEEP_CONNECTION)) {
			return sink->ResponseConnection(kgl_expand_string("close"));
		}
		if (http_major == 1 && http_minor >= 1) {
			//HTTP/1.1 default keep-alive
			return true;
		}
		return sink->ResponseConnection(kgl_expand_string("keep-alive"));
	}
	bool responseHeader(const char *name,hlen_t name_len,const char *val,hlen_t val_len);
	//������header��ʼ����bodyʱ����
	bool startResponseBody(INT64 body_len);
	inline bool needFilter() {
		return of_ctx!=NULL;
	}
	char *BuildVary(const char *vary);
	
	bool ParseHeader(const char *attr, int attr_len,char *val,int val_len, bool is_first);
	const char *getMethod();
	int Write(const char *buf, int len);
	bool WriteAll(const char *buf, int len);
	bool WriteBuff(kbuf *buf);
	int checkFilter(KHttpObject *obj);
	//����(����)
	KSpeedLimitHelper *slh;
	void pushSpeedLimit(KSpeedLimit *sl)
	{
		KSpeedLimitHelper *helper = new KSpeedLimitHelper(sl);
		helper->next = slh;
		slh = helper;
	}
	int getSleepTime(int len)
	{
		int sleepTime = 0;
		KSpeedLimitHelper *helper = slh;
		while (helper) {
			int t = helper->sl->getSleepTime(len);
			if (t>sleepTime) {
				sleepTime = t;
			}
			helper = helper->next;
		}
		return sleepTime;
	}
	uint16_t GetSelfPort()
	{
		if (self_port > 0) {
			return self_port;
		}
		return sink->GetSelfPort();
	}
	void SetSelfPort(uint16_t port, bool ssl);
	//�ͻ���ʵip(�п��ܱ��滻)
	const char *getClientIp()
	{
		if (client_ip) {
			return client_ip;
		}
		client_ip = (char *)malloc(MAXIPLEN);
		sockaddr_i *addr = sink->GetAddr();
		ksocket_sockaddr_ip(addr, client_ip, MAXIPLEN);
		return client_ip;
	}
	char *client_ip;
	//��������ʱ���󶨵ı���ip
	char *bind_ip;
	//������,chΪ�����������ch_connectΪ���ӽ�������
	kgl_pool_t *pool;	
	void registerRequestCleanHook(kgl_pool_cleanup_pt callBack,void *data)
	{
		assert(pool);
		kgl_pool_cleanup_t *cn = kgl_pool_cleanup_add(pool, 0);
		cn->data = data;
		cn->handler = callBack;
	}
	void registerConnectCleanHook(kgl_pool_cleanup_pt callBack,void *data)
	{
		kgl_pool_cleanup_t *cn = kgl_pool_cleanup_add(sink->GetConnectionPool(), 0);
		cn->data = data;
		cn->handler = callBack;
	}
	//����ͳ��
	KFlowInfoHelper *fh;
	void AddUpFlow(int flow)
	{
		KFlowInfoHelper *helper = fh;
		while (helper) {
			helper->fi->AddUpFlow((INT64)flow);
			helper = helper->next;
		}
	}
	void AddDownFlow(int flow,bool is_header_length=false)
	{
		if (!is_header_length) {
			send_size += flow;
		}
		KFlowInfoHelper *helper = fh;
		while (helper) {
			helper->fi->AddDownFlow((INT64)flow, ctx->cache_hit);
			helper = helper->next;
		}
	}
	void pushFlowInfo(KFlowInfo *fi)
	{
		KFlowInfoHelper *helper = new KFlowInfoHelper(fi);
		helper->next = fh;
		fh = helper;
	}
	uint8_t GetWorkModel()
	{
		kserver *server = sink->GetBindServer();
		if (server == NULL) {
			return 0;
		}
		return server->flags;
	}
	kev_result NextFetchObject(KRequestQueue *queue);
	void pushFetchObject(KFetchObject *fo);
	void appendFetchObject(KFetchObject *fo);
	bool hasFinalFetchObject();
#if 0
	bool HasWorkModel(int work_model)
	{
		kserver *server = sink->GetBindServer();
		if (server == NULL) {
			return false;
		}
		return TEST(server->flags, work_model) > 0;
	}
	bool IsWorkModel(int work_model)
	{
		kserver *server = sink->GetBindServer();
		if (server == NULL) {
			return false;
		}
		return TEST(server->flags, work_model) == work_model;
	}
#endif
	
#ifdef ENABLE_REQUEST_QUEUE
	KRequestQueue *queue;
#endif
	//�Ӷ��Ϸ����ڴ棬��rqɾ��ʱ���Զ��ͷš�
	void *alloc_connect_memory(int size)
	{
		return kgl_pnalloc(sink->GetConnectionPool(), size);
	}
	void *alloc_request_memory(int size)
	{
		return kgl_pnalloc(pool, size);
	}

	const char *GetHttpValue(const char *attr)
	{
		KHttpHeader *next = header;
		while (next) {
			if (!strcasecmp(attr, next->attr)) {
				return next->val;
			}
			next = next->next;
		}
		return NULL;
	}
	bool IsSync()
	{
		return TEST(flags, RQ_SYNC);
	}
	void AddSync()
	{
		sink->AddSync();
		SET(flags, RQ_SYNC);
	}
	void RemoveSync()
	{
		sink->RemoveSync();
		CLR(flags, RQ_SYNC);
	}
	int Read(char *buf, int len);
	kev_result WriteExpectedCallback(void *arg, result_callback result, buffer_callback buffer);
	kev_result Read(void *arg, result_callback result, buffer_callback buffer);
	kev_result Write(void *arg, result_callback result, buffer_callback buffer);
	void *GetCurrentWriteHookContext()
	{
		kassert(write_stack && write_stack->hook_head);
		return write_stack->hook_head->arg;
	}
	bool HasWriteHook()
	{
		return write_stack && write_stack->hook_head;
	}
	void AddWriteHook(void *arg, KHttpRequestWriteHook result,bool last);
	kev_result WriteHookCallBack();
private:
	kev_result LowRead(void *arg, result_callback result, buffer_callback buffer);
	void InitPool(kgl_pool_t *pool) {
		kassert(this->pool == NULL);
		this->pool = pool;
		if (this->pool == NULL) {
			this->pool = kgl_create_pool(KGL_REQUEST_POOL_SIZE);
		}
	}
	kgl_header_result InternalParseHeader(const char *attr, int attr_len, char *val,int *val_len, bool is_first);
	bool parseMeth(const char *src);
	bool parseConnectUrl(char *src);
	bool parseHttpVersion(char *ver);
	kgl_header_result parseHost(char *val);
#ifdef ENABLE_REQUEST_QUEUE
	void ReleaseQueue();
#endif
};
struct RequestError
{
	int code;
	const char *msg;
	void set(int code,const char *msg)
	{
		this->code = code;
		this->msg = msg;
	}
};
inline u_short string_hash(const char *p,int len, u_short res) {
    while(*p && len>0){
        --len;
        res = res*3 + (*p);
        p++;
    }
    return res;
}
inline u_short string_hash(const char *p, u_short res) {
        int i = 8;
        while(*p && i){
                --i;
                res *= *p;
                p++;
        }
        return res;
        /*
        if (p && *p) {
                //p = p + strlen(p) - 1;
                i = 8;
                while ((p >= str) && i) {
                        i--;
                        res += *p * *p;
                        p--;
                }
        }
        return res;
        */
}
kev_result stageWriteRequest(KHttpRequest *rq, KAutoBuffer *buffer);
/**
* ���뷢�����ݣ�����ָ����kbuf
*/
kev_result stageWriteRequest(KHttpRequest *rq,kbuf *buf,int start,int len);
kev_result handleStartRequest(KHttpRequest *rq, int header_length);
#ifdef ENABLE_TF_EXCHANGE
kev_result startTempFileWriteRequest(KHttpRequest *rq);
kev_result stageTempFileWriteEnd(KHttpRequest *rq);
#endif
#endif
