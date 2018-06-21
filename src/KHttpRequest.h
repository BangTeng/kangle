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
#include "KSocket.h"
#include "KMutex.h"
#include "KAcserver.h"
#include "KHttpProtocolParserHook.h"
#include "KHttpProtocolParser.h"
#include "KHttpHeader.h"
#include "KBuffer.h"
#include "KReadWriteBuffer.h"
#include "KDomainUser.h"
#include "KSendable.h"
#include "KString.h"
#include "KHttpAuth.h"
#include "do_config.h"
#include "KContext.h"
#include "KResponseContext.h"
#include "KUrl.h"
#include "KFileName.h"
#include "KSpeedLimit.h"
#include "KTempFile.h"
#include "KInputFilter.h"
#include "KServer.h"
#include "KFlowInfo.h"
#include "KHttp2.h"
#include "KMemPool.h"
#include "KConnectionSelectable.h"
#define KGL_REQUEST_POOL_SIZE 4096
#ifdef ENABLE_STAT_STUB
extern volatile uint64_t kgl_total_requests ;
extern volatile uint64_t kgl_total_accepts;
extern volatile uint64_t kgl_total_servers;
extern volatile uint32_t kgl_reading;
extern volatile uint32_t kgl_writing;
extern volatile uint32_t kgl_waiting;
extern volatile uint32_t kgl_aio_count;
#endif
#define READ_BUFF_SZ	8192

class KFetchObject;
class KSubVirtualHost;
class KSelector;
class KFilterHelper;
class KHttpObject;
class KAccess;
class KFilterKey;
class KRequestQueue;
class KBigObjectContext;
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
/**
���������󣬿�ʼһ�����������request->beginSubRequest,���������ء�
��һ���ص���������һ��������
�ص�����ֻ�����һ�Ρ����崦����actionָ����
sub_request_free ָʾ����û��������ɣ����������ֻҪ�ͷ�����ڴ�������ɡ�
sub_request_pop  ָʾ�����������ء�������ز�����
*/
enum sub_request_action {
	sub_request_free,
	sub_request_pop
};
typedef void (* sub_request_call_back) (KHttpRequest *rq,void *data,sub_request_action action);

class KSubRequest;
class KContext;
class KOutputFilterContext;
class KHttpFilterContext;

//void WINAPI free_auto_memory(void *arg);
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
	//pre_post�ĳ���
	int pre_post_length;
	unsigned short status_code;
	unsigned short cookie_stick;
	INT64 send_size;
	INT64 begin_time_msec;
	INT64 first_response_time_msec;

};
class KHttpRequest: public KHttpProtocolParserHook,public KStream,public KHttpRequestData {
public:
	inline KHttpRequest(KConnectionSelectable *c)
	{
		stackSize = 0;
		fetchObj = NULL;
		readBuf = NULL;
		readBuf = (char *) xmalloc(READ_BUFF_SZ);
		current_size = READ_BUFF_SZ;
		svh = NULL;
		auth = NULL;
		url = NULL;
		file = NULL;
		of_ctx = NULL;
		client_ip = NULL;
		bind_ip = NULL;
#ifdef ENABLE_INPUT_FILTER
		if_ctx = NULL;
#endif
		
		ctx = new KContext;
		slh = NULL;
#ifdef ENABLE_TF_EXCHANGE
		tf = NULL;
#endif
		sr = NULL;
		meth = 0;
		fh = NULL;
		mark = 0;
#ifdef ENABLE_REQUEST_QUEUE
		queue = NULL;
#endif
#ifdef ENABLE_KSAPI_FILTER
		http_filter_ctx = NULL;
#endif
#ifdef ENABLE_HTTP2
		http2_ctx = NULL;
#endif
		state = STATE_UNKNOW;
		this->c = c;
		pool = NULL;
	}
	inline ~KHttpRequest()
	{		
		close();
#ifdef ENABLE_REQUEST_QUEUE
		assert(queue == NULL);
#endif
		setState(STATE_UNKNOW);
		if (c) {
			c->release(this);
		}
	}
	void close();
	void clean(bool keep_alive=true);
	void init(kgl_pool_t *pool);
	bool isBad();
	char *get_read_buf(int &size);
	char *get_write_buf(int &size);
	void get_write_buf(LPWSABUF buffer,int &bufferCount);
	void set_url_param(char *param);
	//�ж��Ƿ���post���ݿɶ�
	bool has_post_data();
	/*
	 ��post����ʱ�������,����Ҫֱ�ӵ���server->read�ˡ�
	 */
	int read(char *buf, int len);
	std::string getInfo();
	char *getUrl();
	void beginRequest();
	ReadState canRead(int aio_got=0);
	WriteState canWrite(int aio_got=0);
	SOCKET getSockfd() {
		return c->socket->get_socket();
	}
	bool getPeerAddr(ip_addr *addr) {
		c->socket->get_remote_addr(addr);
		return true;
	}
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
	void endParse();
	//bool closeConnection();
	void closeFetchObject(bool destroy=true);
	void resetFetchObject();

	void freeUrl();
	bool rewriteUrl(const char *newUrl, int errorCode = 0,const char *prefix = NULL);
	u_char http_major;
	u_char http_minor;
	u_char meth;
	u_char state;
	char *hot;
	char *readBuf;
	size_t current_size;
	void setState(u_char state) {
#ifdef ENABLE_STAT_STUB
		if (this->state==state) {
			return;
		}
		switch (this->state) {
		case STATE_IDLE:
		case STATE_QUEUE:
			katom_dec((void *)&kgl_waiting);
			break;
		case STATE_RECV:
			katom_dec((void *)&kgl_reading);
			break;
		case STATE_SEND:
			katom_dec((void *)&kgl_writing);
			break;
		}
#endif
		this->state = state;
#ifdef ENABLE_STAT_STUB
		switch (state) {
		case STATE_IDLE:
		case STATE_QUEUE:
			katom_inc((void *)&kgl_waiting);
			break;
		case STATE_RECV:
			katom_inc((void *)&kgl_reading);
			break;
		case STATE_SEND:
			katom_inc((void *)&kgl_writing);
			break;
		}
#endif		
	}
	const char *getState() {
		switch (state) {
		case STATE_IDLE:
			return "idle";
		case STATE_SEND:
			return "send";
		case STATE_RECV:
			return "recv";
		case STATE_QUEUE:
			return "queue";
		}
		return "unknow";
	}
	KConnectionSelectable *c;
#ifdef ENABLE_TF_EXCHANGE
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
	//�������
	KReadWriteBuffer buffer;
	//����Դ
	KFetchObject *fetchObj;
	//�����ļ�ӳ��
	KFileName *file;
	//��������
	KSubVirtualHost *svh;
	void releaseVirtualHost();
	//������
	KSubRequest *sr;
	/*
	 * ԭʼurl
	 */
	KUrl raw_url;
	KUrl *url;
	//http��֤
	KHttpAuth *auth;
	//����httpЭ�����
	KHttpProtocolParser parser;
	//�й�object������������
	KContext *ctx;
	//����������
	KResponseContext send_ctx;
#ifdef ENABLE_INPUT_FILTER
	bool hasInputFilter()
	{
		if (if_ctx==NULL) {
			return false;
		}
		return !if_ctx->isEmpty();
	}
	KDechunkEngine *getDechunkEngine() {
		if (if_ctx==NULL) {
			if_ctx = new KInputFilterContext(this);
		}
		if (if_ctx->dechunk==NULL) {
			if_ctx->dechunk = new KDechunkEngine;
		}
		return if_ctx->dechunk;
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
		if (left_read>0 || TEST(c->st_flags,STF_NO_KA)) {
			//����post����û�ж���,����ر�����
			SET(flags,RQ_CONNECTION_CLOSE);
		}
		setState(STATE_SEND);
		this->status_code = status_code;
		first_response_time_msec = kgl_current_msec;
#ifdef ENABLE_HTTP2
		if (http2_ctx) {
			return c->http2->add_status(http2_ctx,status_code);
		}
#endif
		return true;
	}
	inline bool responseHeader(know_http_header name,const char *val,hlen_t val_len)
	{
#ifdef ENABLE_HTTP2
		if (http2_ctx != NULL) {
			return c->http2->add_header(http2_ctx,name, val, val_len);
		}
#endif
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
#ifdef ENABLE_HTTP2
		if (http2_ctx !=NULL ) {
			return false;
		}
#endif
		if (ctx->connection_upgrade) {
			responseHeader(kgl_expand_string("Connection"), kgl_expand_string("upgrade"));
			return false;
		} else if (TEST(flags, RQ_CONNECTION_CLOSE) || !TEST(flags, RQ_HAS_KEEP_CONNECTION)) {
			responseHeader(kgl_expand_string("Connection"), kgl_expand_string("close"));
			return false;
		} else {			
			responseHeader(kgl_expand_string("Connection"), kgl_expand_string("keep-alive"));
			return true;
		}
	}
	bool responseHeader(const char *name,hlen_t name_len,const char *val,hlen_t val_len);
	//������header��ʼ����bodyʱ����
	void startResponseBody(INT64 body_len);
	inline bool needFilter() {
		return of_ctx!=NULL;
	}
	
	int parseHeader(const char *attr, char *val,int &val_len, bool isFirst);
	const char *getMethod();
	void getCharset(KHttpHeader *header);
	StreamState write_all(const char *buf, int len);

	//�첽���ã����������󣬷���ʱ���Ǵ�fetchObj->open��
	void beginSubRequest(KUrl *url,sub_request_call_back callBack,void *data);
	void endSubRequest();
	int checkFilter(KHttpObject *obj);
	u_short workModel;
	/*
	 * stackSizeָʾssiָʾ�ڲ�����������
	 *
	 */
	unsigned char stackSize;
	unsigned char mark;

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
	//�ͻ���ʵip(�п��ܱ��滻)
	char *getClientIp()
	{
		if (client_ip) {
			return client_ip;
		}
		client_ip = (char *)malloc(MAXIPLEN);
		c->socket->get_remote_ip(client_ip,MAXIPLEN);
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
		kgl_pool_cleanup_t *cn = kgl_pool_cleanup_add(c->get_pool(), 0);
		cn->data = data;
		cn->handler = callBack;
	}
#ifdef ENABLE_KSAPI_FILTER
	KHttpFilterContext *http_filter_ctx;
	void init_http_filter();
#endif
	//����ͳ��
	KFlowInfoHelper *fh;
	void addFlow(INT64 flow,bool cache_hit)
	{
		KFlowInfoHelper *helper = fh;
		while (helper) {
			helper->fi->addFlow(flow, cache_hit);
			helper = helper->next;
		}
	}
	void pushFlowInfo(KFlowInfo *fi)
	{
		KFlowInfoHelper *helper = new KFlowInfoHelper(fi);
		helper->next = fh;
		fh = helper;
	}
#ifdef ENABLE_HTTP2
	//spdyЭ���õ���������,����������KHttp2����
	KHttp2Context *http2_ctx;
#endif
	
#ifdef ENABLE_REQUEST_QUEUE
	KRequestQueue *queue;
#endif
	//�Ӷ��Ϸ����ڴ棬��rqɾ��ʱ���Զ��ͷš�
	void *alloc_connect_memory(int size)
	{
		return kgl_pnalloc(c->get_pool(), size);
	}
	void *alloc_request_memory(int size)
	{
		return kgl_pnalloc(pool, size);
	}
	bool sync_send_header();
	bool sync_send_buffer();
private:
	bool parseMeth(const char *src);
	bool parseConnectUrl(char *src);
	bool parseHttpVersion(char *ver);
	int parseHost(char *val);
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
/**
* ���뷢�����ݣ�����rq->buffer
*/
void stageWriteRequest(KHttpRequest *rq);
/**
* ���뷢�����ݣ�����ָ����buff
*/
void stageWriteRequest(KHttpRequest *rq,buff *buf,int start,int len);
void startTempFileWriteRequest(KHttpRequest *rq);
#endif
