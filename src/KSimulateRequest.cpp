#include "KSimulateRequest.h"
#include "KHttpRequest.h"
#include "http.h"
#include "kselector_manager.h"
#include "KHttpProxyFetchObject.h"
#include "KDynamicListen.h"
#include "KTimer.h"
#include "KGzip.h"
#include "kmalloc.h"
#ifdef ENABLE_SIMULATE_HTTP
kev_result next_async_http_request(void *arg, int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	if (got==1) {
		return handleStartRequest(rq, 0);
	}
	return async_http_start(rq);
}
int asyncHttpRequest(kgl_async_http *ctx)
{
	if (ctx->postLen>0 && ctx->post==NULL) {
		return 1;
	}
	KSimulateSink *ss = new KSimulateSink;
	KHttpRequest *rq = new KHttpRequest(ss,NULL);
	if (ctx->selector==0) {
		selectable_bind(&ss->c->st, get_perfect_selector());
	} else {
		selectable_bind(&ss->c->st, get_selector_by_index(ctx->selector - 1));
	}
	if (!parse_url(ctx->url,&rq->raw_url)) {
		rq->raw_url.destroy();
		KStringBuf nu;
		nu << ctx->url << "/";
		if (!parse_url(nu.getString(), &rq->raw_url)) {
			delete rq;
			return 1;
		}
	}
	if (rq->raw_url.host==NULL) {
		delete rq;
		return 1;
	}
	if (TEST(rq->raw_url.flags,KGL_URL_ORIG_SSL)) {
		SET(rq->raw_url.flags,KGL_URL_SSL);
	}
	
	if (ctx->host) {
		ss->host = strdup(ctx->host);
	}
	ss->port = ctx->port;
	ss->life_time = ctx->life_time;
	ss->arg = ctx->arg;
	ss->post = ctx->post;
	ss->header = ctx->header;
	ss->body = ctx->body;
	KHttpHeader *head = ctx->rh;
	bool user_agent = false;
	while (head) {
		if (strcasecmp(head->attr,"User-Agent")==0) {
			user_agent = true;
		}
		rq->ParseHeader(head->attr, head->attr_len, head->val, head->val_len, false);
		head = head->next;
	}
	if (!user_agent) {
		//add default user-agent header
		timeLock.Lock();
		rq->ParseHeader(kgl_expand_string("User-Agent"), conf.serverName, conf.serverNameLength, false);
		timeLock.Unlock();
	}
	rq->sink = ss;
	rq->ctx->simulate = 1;
	if (TEST(ctx->flags, KF_SIMULATE_GZIP)) {
		rq->ParseHeader(kgl_expand_string("Accept-Encoding"), kgl_expand_string("gzip"),false);
	}
	rq->meth = KHttpKeyValue::getMethod(ctx->meth);
	rq->content_length = ctx->postLen;
	rq->http_major = 1;
	rq->http_minor = 1;
	SET(rq->flags,RQ_CONNECTION_CLOSE);
	if (!TEST(ctx->flags, KF_SIMULATE_CACHE)) {
		SET(rq->flags, RQ_HAS_NO_CACHE);
		SET(rq->filter_flags, RF_NO_CACHE);
	}
	if (rq->content_length>0) {
		SET(rq->flags,RQ_HAS_CONTENT_LEN);
	}
	int next_got = 0;
	if (TEST(ctx->flags,KF_SIMULATE_LOCAL)) {
		ss->c->server = conf.gvm->refsServer(rq->raw_url.port);
		if (ss->c->server == NULL) {
			ss->body = NULL;
			delete rq;
			return 2;
		}
		next_got = 1;
	} else {
		rq->beginRequest();
		rq->fetchObj = new KHttpProxyFetchObject();
		rq->ctx->skip_access = 1;
	}
	kgl_selector_module.next(ss->c->st.selector,next_async_http_request, rq, next_got);
	return 0;
}
int WINAPI test_header_hook(void *arg,int code,KHttpHeader *header)
{
	return 0;
}
int WINAPI test_body_hook(void *arg,const char *data,int len)
{
	//printf("len = %d\n",len);
	if (data) {
		//fwrite(data, 1, len, stdout);
	}
	return 0;
}
int WINAPI test_post_hook(void *arg,char *buf,int len)
{
	kgl_memcpy(buf,"test",4);
	return 4;
}
static void WINAPI timer_simulate(void *arg)
{
	test_simulate_request();
}

KSimulateSink::KSimulateSink()
{
	memset(&header_manager, 0, sizeof(KHttpHeaderManager));
	exptected_done = 0;
	status_code = 0;
	host = NULL;
	body = NULL;
	sockaddr_i addr;
	ksocket_getaddr("127.0.0.1", 0, PF_INET, AI_NUMERICHOST, &addr);
	c = kconnection_new(&addr);
}
KSimulateSink::~KSimulateSink()
{
	if (host) {
		xfree(host);
	}
	if (body) {
		body(arg,NULL,exptected_done);
	}
	kconnection_destroy(c);
}
void KSimulateSink::EndRequest(KHttpRequest *rq)
{
	exptected_done = !rq->ctx->body_not_complete;
	delete rq;
}
typedef struct {
	char *save_file;
	int code;
	result_callback cb;
	void *arg;
	KWStream *st;
} async_download_worker;

int WINAPI async_download_header_hook(void *arg, int code, struct KHttpHeader *header)
{
	async_download_worker *dw = (async_download_worker *)arg;
	dw->code = code;
	kassert(dw->st == NULL);
	if (code == 200) {
		KFileStream *fs = new KFileStream;
		fs->last_modified = 0;
		KStringBuf filename;
		filename << dw->save_file << ".tmp";
		if (!fs->open(filename.getString())) {
			delete fs;
			return 1;
		}
		dw->st = fs;
		while (header) {
			if (is_attr(header, "Content-Encoding") && is_val(header, kgl_expand_string("gzip"))) {
				KGzipDecompress *st = new KGzipDecompress(false, dw->st, true);
				dw->st = st;				
			} else if (is_attr(header, "Last-Modified")) {				
				fs->last_modified = parse1123time(header->val);
				//printf("Last-Modified time=[%x] [%s]\n", (unsigned)fs->last_modified,header->val);
			}
			header = header->next;
		}
	}
	return 0;
}
int WINAPI async_download_body_hook(void *arg, const char *data, int len)
{
	async_download_worker *dw = (async_download_worker *)arg;
	if (data == NULL) {
		if (dw->st) {
			dw->st->write_end();
			delete dw->st;
			dw->st = NULL;
			KStringBuf filename;
			filename << dw->save_file << ".tmp";
			if (len == 1 && (dw->code==200 || dw->code==206)) {
				unlink(dw->save_file);
				if (0 != rename(filename.getString(), dw->save_file)) {
					dw->code += 2000;
				}
			} else {
				unlink(filename.getString());
			}
		}		
		if (len == 0 && (dw->code>=200 && dw->code<300)) {
			dw->code += 1000;
		}
		if (dw->cb) {
			dw->cb(dw->arg, dw->code);
		}
		if (dw->save_file) {
			xfree(dw->save_file);
		}
		xfree(dw);
	}
	if (data && dw->st) {
		if (STREAM_WRITE_SUCCESS != dw->st->write_all(data, len)) {
			return 1;
		}
	}
	return 0;
}
void async_download(const char *url, const char *file, result_callback cb, void *arg)
{
	async_download_worker *download_ctx = xmemory_new(async_download_worker);
	memset(download_ctx, 0, sizeof(async_download_worker));
	download_ctx->save_file = strdup(file);
	download_ctx->cb = cb;
	download_ctx->arg = arg;
	kgl_async_http ctx;
	memset(&ctx, 0, sizeof(ctx));
	struct stat buf;
	int ret = stat(file, &buf);
	char tmp_buf[42];
	KHttpHeader header;
	memset(&header, 0, sizeof(header));
	if (ret == 0) {
		mk1123time(buf.st_mtime, tmp_buf, 41);
		header.attr = (char *)"If-Modified-Since";
		header.attr_len = (hlen_t)strlen(header.attr);
		header.val = tmp_buf;
		header.val_len = 41;
		//printf("if-modified-since time=[%x] [%s]\n", (unsigned)buf.st_mtime,tmp_buf);
		ctx.rh = &header;
	}
	ctx.arg = download_ctx;
	ctx.url = url;
	ctx.meth = "GET";
	ctx.postLen = 0;
	ctx.flags = KF_SIMULATE_GZIP;
	ctx.header = async_download_header_hook;
	ctx.body = async_download_body_hook;
	ctx.post = NULL;
	if (0 != asyncHttpRequest(&ctx) && cb) {
		cb(arg, -1);
	}
}
void test_simulate_callback(void *arg, int code)
{
	printf("status_code=[%d]\n", code);
}
bool test_simulate_request()
{
	//async_download("https://www.cdnbest.com/public/view/default/js/global.js", "d:\\test.js", test_simulate_callback, NULL);
	return true;
	timer_run(timer_simulate, NULL, 2000, 0);
	kgl_async_http ctx;
	memset(&ctx, 0, sizeof(ctx));
	ctx.url = "http://test.monitor.dnsdun.com:1112/monitor?a=status_all&name=test&version=2.2";
	ctx.meth = "get";
	ctx.postLen = 0;
	//ctx.header = test_header_hook;
	ctx.flags = KF_SIMULATE_DELTA | KF_SIMULATE_GZIP;
	ctx.body = test_body_hook;
	ctx.arg = NULL;
	ctx.rh = NULL;
	ctx.post = test_post_hook;
	asyncHttpRequest(&ctx);
	//asyncHttpRequest(METH_GET,"http://www.kangleweb.net/test.php",NULL,test_header_hook,test_body_hook,NULL);
	return true;
}
#endif

