#include "KAsyncFetchObject.h"
#include "http.h"
#include "KSelector.h"
#include "KAsyncWorker.h"
#ifdef _WIN32
#include "KIOCPSelector.h"
#endif
#include "KSingleAcserver.h"
#include "KCdnContainer.h"
#include "KSimulateRequest.h"
#include "KHttpProxyFetchObject.h"
#include "KHttp2.h"

#ifdef ENABLE_UPSTREAM_SSL
void resultSSLConnect(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	assert(rq->fetchObj);
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	if (got<0) {
		fo->handleUpstreamError(rq,STATUS_BAD_GATEWAY,"cann't connect to host use ssl",got);
		return;
	}
	assert(fo->client);
	assert(fo->client->socket);
	KSSLSocket *socket = static_cast<KSSLSocket *>(fo->client->socket);
	ssl_status status = socket->handshake();
#ifdef ENABLE_KSSL_BIO
	if (status!=ret_error && BIO_pending(socket->ssl_bio[WRITE_PIPE].bio)>0) {
		fo->client->upstream_write(rq, resultSSLConnect, NULL);
		return;
	}
#endif
	switch(status) {
	case ret_ok:
	{
		
		fo->sendHead(rq);
	}
		return;
	case ret_want_read:
#ifndef ENABLE_KSSL_BIO
		fo->client->clear_flag(STF_RREADY);
#endif
		fo->client->upstream_read(rq,resultSSLConnect,NULL);
		return;
	case ret_want_write:
#ifndef ENABLE_KSSL_BIO
		fo->client->clear_flag(STF_WREADY);
#endif
		fo->client->upstream_write(rq,resultSSLConnect,NULL);
		return;
	default:
		fo->handleUpstreamError(rq,STATUS_BAD_GATEWAY,"cann't connect to host use ssl",got);
		return;	
	}
}
#endif
void next_connection_result(void *arg, int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	fo->connect_result(rq, got>0);
}
void proxyConnect(KHttpRequest *rq)
{
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	const char *ip = rq->bind_ip;
	KUrl *url = (TEST(rq->filter_flags,RF_PROXY_RAW_URL)?&rq->raw_url:rq->url);
	const char *host = url->host;
	u_short port = url->port;
	bool isIp = false;
	const char *ssl = NULL;
	int life_time = 2;
#ifdef IP_TRANSPARENT
#ifdef ENABLE_TPROXY
	char mip[MAXIPLEN];
	if (TEST(rq->workModel,WORK_MODEL_TPROXY) && TEST(rq->filter_flags,RF_TPROXY_TRUST_DNS)) {
		if (TEST(rq->filter_flags,RF_TPROXY_UPSTREAM)) {
			if (ip==NULL) {
				ip = rq->getClientIp();
			}
		}
		sockaddr_i s_sockaddr;
		socklen_t addr_len = sizeof(sockaddr_i);
		::getsockname(rq->c->socket->get_socket(), (struct sockaddr *) &s_sockaddr, &addr_len);
		KSocket::make_ip(&s_sockaddr, mip, MAXIPLEN);
		host = mip;
#ifdef KSOCKET_IPV6
		if (s_sockaddr.v4.sin_family == PF_INET6) {
			port = ntohs(s_sockaddr.v6.sin6_port);
		} else
#endif
		port = ntohs(s_sockaddr.v4.sin_port);
		isIp = true;
	}
#endif
#endif
	if (TEST(url->flags,KGL_URL_SSL)) {
		ssl = "s";
	}
#ifdef ENABLE_SIMULATE_HTTP
	/* simuate request must replace host and port */
	if (TEST(rq->workModel,WORK_MODEL_SIMULATE)) {
		KSimulateSocket *ss = static_cast<KSimulateSocket *>(rq->c->socket);
		if (ss->host && *ss->host) {
			host = ss->host;
			if (ss->port>0) {
				port = ss->port;
			}
			life_time = ss->life_time;
		}
	}
#endif
	KRedirect *sa = cdnContainer.refsRedirect(ip,host,port,ssl,life_time,Proto_http,isIp);
	if (sa==NULL) {
		fo->connectCallBack(rq,NULL);
	} else {
		sa->connect(rq);
		sa->release();
	}
}
void bufferUpStreamReadBodyResult(void *arg,LPWSABUF buf,int &bufCount)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	bufCount = 1;
	int len;
	buf[0].iov_base = (char *)fo->getBodyBuffer(rq,len);
	buf[0].iov_len = len;
	return;
}
void resultUpStreamReadBodyResult(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	fo->handleReadBody(rq,got);
}
void resultUpstreamReadPost(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	fo->handleReadPost(rq,got);
}
void bufferUpstreamReadPost(void *arg,LPWSABUF buf,int &bufCount)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	int len;
	buf[0].iov_base = (char *)fo->getPostWBuffer(rq,len);
	buf[0].iov_len = len;
	bufCount = 1;
}
void resultUpstreamSendPost(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	fo->handleSendPost(rq,got);
}
void bufferUpstreamSendPost(void *arg,LPWSABUF buf,int &bufCount)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	fo->getPostRBuffer(rq,buf,bufCount);
}
void resultUpstreamSendHead(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	fo->handleSendHead(rq,got);
}
void request_connection_broken(void *arg,int got)
{
	//printf("connection broken\n");
	KHttpRequest *rq = (KHttpRequest *)arg;
	rq->ctx->read_huped = true;
	if (rq->fetchObj==NULL) {
		return;
	}
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	if (fo->client) {
		fo->client->upstream_shutdown();
	}
}
void bufferUpstreamSendHead(void *arg,LPWSABUF buf,int &bufCount)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	fo->buffer->getRBuffer(buf,bufCount);
}
void resultUpstreamReadHead(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	fo->handleReadHead(rq,got);
}
void bufferUpstreamReadHead(void *arg,LPWSABUF buf,int &bufCount)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	int len;
	buf[0].iov_base = (char *)fo->getHeadRBuffer(rq,len);
	buf[0].iov_len = len;
	bufCount = 1;
	return;
}
void resultUpstreamConnectResult(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	fo->handleConnectResult(rq,got);
}
void resultUpstreamHttp2ReadHeader(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	fo->handleHttp2ReadHead(rq,got);
}
void KAsyncFetchObject::handleConnectResult(KHttpRequest *rq,int got)
{
	if (got==-1) {
		handleUpstreamError(rq,STATUS_GATEWAY_TIMEOUT,"connect to remote host time out",got);
		return;
	}
#ifdef ENABLE_UPSTREAM_SSL
	if (client->isSSL()) {
#ifdef _WIN32
		client->socket->setnoblock();
#endif
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
		if (!TEST(rq->filter_flags, RF_UPSTREAM_NOSNI)) {
			static_cast<KSSLSocket *>(client->socket)->setHostName(rq->url->host);
		}
#endif
		resultSSLConnect(rq,0);
		return;
	}
#endif
	sendHead(rq);
}
void KAsyncFetchObject::retryOpen(KHttpRequest *rq)
{
	if (client) {
		client->gc(-1);
		client = NULL;
	}
	if (buffer) {
		delete buffer;
		buffer = NULL;
	}
	hot = header;
	open(rq);
}
void KAsyncFetchObject::open(KHttpRequest *rq)
{
	lifeTime = -1;
	//默认上流支持长连接
	rq->ctx->upstream_connection_keep_alive = true;
	KFetchObject::open(rq);
	if (brd==NULL) {
		proxyConnect(rq);
		return;
	}
	if (!brd->rd->enable) {
		handleError(rq,STATUS_SERVICE_UNAVAILABLE,"extend is disable");
		return ;
	}
	badStage = BadStage_Connect;
	brd->rd->connect(rq);
}
void KAsyncFetchObject::read_hup(KHttpRequest *rq)
{
	if (!conf.read_hup) {
		return;
	}
     
	rq->c->read_hup(rq, request_connection_broken);
}
void KAsyncFetchObject::connect_result(KHttpRequest *rq, bool half_connection)
{
	assert(rq->c->selector->is_same_thread());
	if (this->client == NULL) {
		handleError(rq, STATUS_GATEWAY_TIMEOUT, "Cann't connect to remote host");
		return;
	}
	client->tmo = rq->c->tmo;
	if (client->selector == NULL) {
		client->selector = rq->c->selector;
	}
	read_hup(rq);
	if (half_connection) {
		client->connect(rq, resultUpstreamConnectResult);
	} else {
		sendHead(rq);
	}
}
void KAsyncFetchObject::connectCallBack(KHttpRequest *rq,KUpstreamSelectable *client,bool half_connection)
{
	assert(this->client == NULL);
	this->client = client;
	if (rq->c->selector->is_same_thread()) {
		connect_result(rq, half_connection);
		return;
	}
	rq->c->selector->next(next_connection_result, rq, half_connection);
}
void KAsyncFetchObject::sendHead(KHttpRequest *rq)
{
	
	badStage = BadStage_TrySend;
	if (buffer==NULL) {
		buildHead(rq);
		assert(buffer);
		
	}
	
	unsigned len = buffer->startRead();
	if (len == 0) {
		handleError(rq, STATUS_SERVER_ERROR, "cann't build head");
		return;
	}
	client->socket->set_delay();
	client->upstream_write(rq,resultUpstreamSendHead,bufferUpstreamSendHead);
}
void KAsyncFetchObject::continueReadBody(KHttpRequest *rq)
{
	if (rq->ctx->connection_upgrade) {
		client->upstream_read(rq, resultUpStreamReadBodyResult, bufferUpStreamReadBodyResult);
		return;
	}
	if (!checkContinueReadBody(rq)) {
		stage_rdata_end(rq,STREAM_WRITE_SUCCESS);
		return;
	}
	read_hup(rq);
	client->upstream_read(rq,resultUpStreamReadBodyResult,bufferUpStreamReadBodyResult);
}
void KAsyncFetchObject::readBody(KHttpRequest *rq)
{
	int bodyLen;
	for (;;) {
		char *body = nextBody(rq,bodyLen);
		if (body==NULL) {
			break;
		}
		if (!pushHttpBody(rq,body,bodyLen)) {
			return;
		}
	}
	continueReadBody(rq);
}
void KAsyncFetchObject::handleReadBody(KHttpRequest *rq,int got)
{
	if (got<=0) {
		if (rq->ctx->connection_upgrade) {
			shutdown(rq);
			return;
		}
		
		lifeTime = -1;
		stage_rdata_end(rq,STREAM_WRITE_SUCCESS);
		return;
	}
	assert(header);
	parseBody(rq,header,got);
	readBody(rq);
}
void KAsyncFetchObject::handleHttp2ReadHead(KHttpRequest *rq,int got)
{
	if (got<0) {
		handleUpstreamError(rq,STATUS_SERVER_ERROR,"Cann't Send head to remote server",got);
		return;
	}
	rq->ctx->obj->data->headers = client->parser->stealHeaders(rq->ctx->obj->data->headers);
	assert(got==0);
	badStage = BadStage_SendSuccess;
	readHeadSuccess(rq);
}
bool KAsyncFetchObject::try_pre_load_body(KHttpRequest *rq)
{
	if (rq->pre_post_length>0) {
		int buf_size;
		char *buf = getPostWBuffer(rq,buf_size);
		int copy_size = MIN(buf_size,rq->pre_post_length);
		memcpy(buf,rq->parser.body,copy_size);
		rq->parser.bodyLen -= copy_size;
		rq->parser.body += copy_size;
		rq->pre_post_length -= copy_size;
		handleReadPost(rq,copy_size);
		return true;
	}
	return false;
}
void KAsyncFetchObject::sendHeadSuccess(KHttpRequest *rq)
{
	buffer->destroy();
	if (rq->left_read!=0) {
		//handle post data
		if (try_pre_load_body(rq)) {
			return;
		}
		//read post data
		readPost(rq);
		return;
	}
	//发送头成功,无post数据处理.
	startReadHead(rq);
}
void KAsyncFetchObject::startReadHead(KHttpRequest *rq)
{
	
	client->socket->set_nodelay();
	client->upstream_read(rq,resultUpstreamReadHead,bufferUpstreamReadHead);
}
void KAsyncFetchObject::handleSendHead(KHttpRequest *rq,int got)
{
	//printf("handleSendHead got=[%d]\n",got);
	if (got<=0) {
		handleUpstreamError(rq,STATUS_SERVER_ERROR,"Cann't Send head to remote server",got);
		return;
	}
	badStage = BadStage_SendSuccess;
	if(buffer->readSuccess(got)){
		//continue send head
		client->upstream_write(rq,resultUpstreamSendHead,bufferUpstreamSendHead);
		return;
	}
	sendHeadSuccess(rq);
}
void KAsyncFetchObject::handleSendPost(KHttpRequest *rq,int got)
{
	if (got<=0) {
		handleUpstreamError(rq,STATUS_SERVER_ERROR,"cann't send post data to remote server",got);
		return;
	}
	if (buffer->readSuccess(got)) {
		client->upstream_write(rq,resultUpstreamSendPost,bufferUpstreamSendPost);
	} else {
		//重置buffer,准备下一次post
		buffer->destroy();
		if (try_pre_load_body(rq)) {
			return;
		}
		//try to read post
		if (!rq->ctx->connection_upgrade && rq->left_read==0) {
			startReadHead(rq);
			return;
		}
		readPost(rq);
	}
}
void KAsyncFetchObject::shutdown(KHttpRequest *rq)
{
	if (!rq->c->is_locked(rq) && !client->is_upstream_locked()) {
		stageEndRequest(rq);
		return;
	}
	client->upstream_shutdown();
	rq->c->shutdown(rq);
}
void KAsyncFetchObject::handleReadPost(KHttpRequest *rq,int got)
{
	//printf("handleReadPost got=[%d]\n",got);
	if (got<=0) {
		if (rq->ctx->connection_upgrade) {
			shutdown(rq);
			return;
		}
		stageEndRequest(rq);
		return;
	}
	if (TEST(rq->flags,RQ_INPUT_CHUNKED)) {
		int len;
		char *buf = buffer->getWBuffer(len);
		bool chunk_is_end = false;
		got = check_chunk_stream(rq,buf,got,chunk_is_end);
		if (chunk_is_end) {
			rq->left_read = 0;
		}
		if (got==0) {			
			startReadHead(rq);
			return;
		}
	} else {
		rq->left_read-=got;
	}
	buffer->writeSuccess(got);
	sendPost(rq);
}
void KAsyncFetchObject::readPost(KHttpRequest *rq)
{
#ifdef ENABLE_TF_EXCHANGE
	if (rq->tf) {
		int got = 0;
		char *tbuf = getPostWBuffer(rq,got);
		got = rq->tf->readBuffer(tbuf,got);
		if (got<=0) {		
			handleUpstreamError(rq,STATUS_SERVER_ERROR,"cann't read post data from temp file",got);
			return;
		}
		rq->left_read-=got;
		buffer->writeSuccess(got);
		sendPost(rq);
		return;
	}
#endif
#ifdef ENABLE_SIMULATE_HTTP
	if (TEST(rq->workModel,WORK_MODEL_SIMULATE)) {
		KSimulateSocket *ss = static_cast<KSimulateSocket *>(rq->c->socket);
		int got = 0;
		char *tbuf = getPostWBuffer(rq,got);
		got = ss->post(ss->arg,tbuf,got);
		if (got<=0) {
			handleUpstreamError(rq,STATUS_SERVER_ERROR,"cann't read post data from temp file",got);
			return;
		}
		rq->left_read-=got;
		buffer->writeSuccess(got);
		sendPost(rq);
		return;
	}
#endif
	rq->c->read(rq,resultUpstreamReadPost,bufferUpstreamReadPost);
}
void KAsyncFetchObject::sendPost(KHttpRequest *rq)
{
	//创建post
	buildPost(rq);
	buffer->startRead();
	if (!rq->ctx->connection_upgrade) {
		read_hup(rq);
	}
	client->upstream_write(rq,resultUpstreamSendPost,bufferUpstreamSendPost);
}
void KAsyncFetchObject::readHeadSuccess(KHttpRequest *rq)
{
	client->isGood();
	if (rq->ctx->connection_upgrade) {
		if (
#ifdef ENABLE_HTTP2
			rq->http2_ctx == NULL &&
#endif
			rq->c->tmo < 10) {
			rq->c->tmo += 5;
		}
		if (
			
			client->tmo < 10) {
			client->tmo += 5;
		}
		rq->c->read(rq, resultUpstreamReadPost, bufferUpstreamReadPost);
	}
	handleUpstreamRecvedHead(rq);
}
void KAsyncFetchObject::handleReadHead(KHttpRequest *rq,int got)
{
	
	char *buf = hot;
	if (got<=0) {
		handleUpstreamError(rq,STATUS_GATEWAY_TIMEOUT,"cann't recv head from remote server",got);
		return;
	}
	assert(hot);
	hot += got;
	switch(parseHead(rq,buf,got)){
		case Parse_Success:
			readHeadSuccess(rq);
			break;
		case Parse_Continue:
			if (current_size > MAX_HTTP_HEAD_SIZE) {
				handleUpstreamError(rq, STATUS_GATEWAY_TIMEOUT, "upstream protocol header size is too big",got);
				break;
			}
			client->upstream_read(rq,resultUpstreamReadHead,bufferUpstreamReadHead);
			break;
		default:
			handleUpstreamError(rq, STATUS_GATEWAY_TIMEOUT, "cann't parse upstream protocol", got);
			break;
	}
}
void KAsyncFetchObject::handleUpstreamError(KHttpRequest *rq,int error,const char *msg,int last_got)
{
	assert(client);
	lifeTime = -1;
	client->isBad(badStage);
	SET(rq->flags, RQ_UPSTREAM_ERROR);
	if (!rq->ctx->connection_connect_proxy && rq->ctx->connection_upgrade) {
		shutdown(rq);
		return;
	}
	
	int err = errno;
	char ips[MAXIPLEN];
	client->socket->get_remote_ip(ips,sizeof(ips));
	char *url = rq->url->getUrl();
	klog(KLOG_WARNING,"rq=[%p] url=[%s] upstream=[%s:%d] error code=[%d],msg=[%s] errno=[%d %s],socket=%d(%s)\n",
		(KSelectable *)rq,
		url,
		ips,
		client->socket->get_remote_port(),
		error,
		msg,
		err,
		strerror(err),
		client->socket->get_socket(),
		(client->isNew()?"new":"pool")
		);
	free(url);
	if (badStage != BadStage_SendSuccess && !client->isNew()) {
		//use pool connectioin will try again
		retryOpen(rq);
		return;
	}
	handleError(rq,error,msg);
}
