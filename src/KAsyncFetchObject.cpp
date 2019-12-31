#include <errno.h>
#include "KAsyncFetchObject.h"
#include "http.h"
#include "kselector.h"
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
kev_result result_ssl_connect(void *arg, int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	assert(rq->fetchObj);
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	if (got < 0) {
		return fo->handleUpstreamError(rq, STATUS_BAD_GATEWAY, "cann't connect to host use ssl", got);
	}
	assert(fo->client);

	return fo->sendHead(rq);
}
#endif
kev_result next_connection_result(void *arg, int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	return fo->connect_result(rq, got>0);
}
#ifdef ENABLE_PROXY_PROTOCOL
kev_result proxyTcpConnect(KHttpRequest *rq)
{
	char ips[MAXIPLEN];
	kconnection *cn = rq->sink->GetConnection();
	if (cn->proxy == NULL || cn->proxy->dst == NULL) {
		return	stageEndRequest(rq);	
	}
	if (!ksocket_sockaddr_ip(cn->proxy->dst, ips, MAXIPLEN-1)) {
		return stageEndRequest(rq);
	}
	uint16_t port = ntohs(cn->proxy->dst->v4.sin_port);
	KSingleAcserver *server = new KSingleAcserver;
	server->proto = Proto_tcp;
	server->sockHelper->setHostPort(ips, port, NULL);
	server->sockHelper->setLifeTime(0);
	return server->connect(rq);
}
#endif
kev_result proxyConnect(KHttpRequest *rq)
{
#ifdef ENABLE_PROXY_PROTOCOL
	if (TEST(rq->GetWorkModel(), WORK_MODEL_PROXY|WORK_MODEL_SSL_PROXY)) {
		return proxyTcpConnect(rq);
	}
#endif
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	const char *ip = rq->bind_ip;
	KUrl *url = (TEST(rq->filter_flags,RF_PROXY_RAW_URL)?&rq->raw_url:rq->url);
	const char *host = url->host;
	u_short port = url->port;
	const char *ssl = NULL;
	int life_time = 2;
#ifdef IP_TRANSPARENT
#ifdef ENABLE_TPROXY
	char mip[MAXIPLEN];
	if (TEST(rq->GetWorkModel(),WORK_MODEL_TPROXY) && TEST(rq->filter_flags,RF_TPROXY_TRUST_DNS)) {
		if (TEST(rq->filter_flags,RF_TPROXY_UPSTREAM)) {
			if (ip==NULL) {
				ip = rq->getClientIp();
			}
		}
		sockaddr_i s_sockaddr;
		socklen_t addr_len = sizeof(sockaddr_i);
		::getsockname(rq->sink->GetConnection()->st.fd, (struct sockaddr *) &s_sockaddr, &addr_len);
		ksocket_sockaddr_ip(&s_sockaddr, mip, MAXIPLEN);
		host = mip;
#ifdef KSOCKET_IPV6
		if (s_sockaddr.v4.sin_family == PF_INET6) {
			port = ntohs(s_sockaddr.v6.sin6_port);
		} else
#endif
		port = ntohs(s_sockaddr.v4.sin_port);
	}
#endif
#endif
	if (TEST(url->flags, KGL_URL_ORIG_SSL)) {
		ssl = "s";
	}
#ifdef ENABLE_SIMULATE_HTTP
	/* simuate request must replace host and port */
	if (rq->ctx->simulate) {
		KSimulateSink *ss = static_cast<KSimulateSink *>(rq->sink);
		if (ss->host && *ss->host) {
			host = ss->host;
			if (ss->port>0) {
				port = ss->port;
			}
			life_time = ss->life_time;
		}
	}
#endif
	KRedirect *sa = server_container->refsRedirect(ip,host,port,ssl,life_time,Proto_http);
	if (sa==NULL) {
		return fo->connectCallBack(rq,NULL);
	}
	kev_result ret = sa->connect(rq);
	sa->release();
	return ret;	
}
kev_result resultUpStreamReadBodyResult(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	return fo->handleReadBody(rq,got);
}
kev_result resultUpstreamReadPost(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	return fo->handleReadPost(rq,got);
}
int bufferUpstreamReadPost(void *arg,LPWSABUF buf,int bufCount)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	int len;
	buf[0].iov_base = (char *)fo->getPostWBuffer(rq,len);
	buf[0].iov_len = len;
	return 1;
}
kev_result resultUpstreamSendPost(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	return fo->handleSendPost(rq,got);
}
int bufferUpstreamSendPost(void *arg,LPWSABUF buf,int bufCount)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	return fo->getPostRBuffer(rq,buf,bufCount);
}
kev_result resultUpstreamSendHead(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	return fo->handleSendHead(rq,got);
}
kev_result request_connection_broken(void *arg,int got)
{
	//printf("connection broken\n");
	KHttpRequest *rq = (KHttpRequest *)arg;
	rq->ctx->read_huped = true;
	if (rq->fetchObj==NULL) {
		return kev_err;
	}
	KUpstream *st = rq->fetchObj->GetUpstream();
	if (st) {
		st->Shutdown();
	}
	return kev_err;
}
int bufferUpstreamSendHead(void *arg,LPWSABUF buf,int bufCount)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	return fo->buffer->getReadBuffer(buf,bufCount);
}
kev_result resultUpstreamReadHead(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	return fo->handleReadHead(rq,got);
}
int bufferUpstreamRead(void *arg,LPWSABUF buf,int bufCount)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	int len;
	buf[0].iov_base = (char *)fo->getUpstreamBuffer(&len);
	buf[0].iov_len = len;
	return 1;
}
kev_result resultUpstreamConnectResult(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	return fo->handleConnectResult(rq,got);
}
kev_result resultUpstreamHttp2ReadHeader(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	return fo->handleHttp2ReadHead(rq,got);
}
kev_result KAsyncFetchObject::handleConnectResult(KHttpRequest *rq,int got)
{
	if (got==-1) {
		return handleUpstreamError(rq,STATUS_GATEWAY_TIMEOUT,"connect to remote host time out",got);
	}
#ifdef ENABLE_UPSTREAM_SSL
	kconnection *c = client->GetConnection();
	if (c->st.ssl) {
		return kconnection_ssl_handshake(c, result_ssl_connect, rq);
	}
#endif
	return sendHead(rq);
}
kev_result KAsyncFetchObject::retryOpen(KHttpRequest *rq)
{
	if (client) {
		client->Gc(-1,0);
		client = NULL;
	}
	if (buffer) {
		delete buffer;
		buffer = NULL;
	}
	if (us_buffer.buf) {
		xfree(us_buffer.buf);
	}
	memset(&us_buffer, 0, sizeof(us_buffer));
	memset(&parser, 0, sizeof(parser));
	return open(rq);
}
kev_result KAsyncFetchObject::open(KHttpRequest *rq)
{
	parser_ctx.keep_alive_time_out = 0;
	rq->ctx->upstream_connection_keep_alive = 1;
	KFetchObject::open(rq);
	if (brd==NULL) {
		return proxyConnect(rq);
	}
	if (!brd->rd->enable) {
		return handleError(rq,STATUS_SERVICE_UNAVAILABLE,"extend is disable");
	}
	badStage = BadStage_Connect;
	return brd->rd->connect(rq);
}
void KAsyncFetchObject::read_hup(KHttpRequest *rq)
{
	if (!conf.read_hup) {
		return;
	}
	rq->sink->ReadHup(rq, request_connection_broken);
}
kev_result KAsyncFetchObject::connect_result(KHttpRequest *rq, bool half_connection)
{
	kassert(kselector_is_same_thread(rq->sink->GetSelector()));
	if (this->client == NULL) {
		return handleError(rq, STATUS_GATEWAY_TIMEOUT, "Cann't connect to remote host");
	}
	client->SetTimeOut(rq->sink->GetTimeOut());
	client->BindSelector(rq->sink->GetSelector());
	read_hup(rq);
	if (half_connection) {
		return client->Connect(rq,resultUpstreamConnectResult);
	}
	return sendHead(rq);
}
kev_result KAsyncFetchObject::connectCallBack(KHttpRequest *rq,KUpstream *client,bool half_connection)
{
	assert(this->client == NULL);
	this->client = client;
	kselector *selector = rq->sink->GetSelector();
	if (kselector_is_same_thread(selector)) {
		return connect_result(rq, half_connection);
	}
	kgl_selector_module.next(selector, next_connection_result, rq, half_connection);
	return kev_ok;
}
kev_result KAsyncFetchObject::sendHead(KHttpRequest *rq)
{
	
	badStage = BadStage_TrySend;
	if (rq->left_read != 0 && !client->IsMultiStream()) {
		if (TEST(rq->flags, RQ_INPUT_CHUNKED)) {
			rq->sink->SetChunkRawRead();
		} else if (rq->left_read == -1) {
			chunk_post = 1;
		}
	}
	if (buffer==NULL) {
		buildHead(rq);
		kassert(buffer);
		
	}
	if (buffer->startRead()==0) {
		return sendHeadSuccess(rq);
	}
	client->SetDelay();
	return client->Write(rq,resultUpstreamSendHead,bufferUpstreamSendHead);
}
kev_result KAsyncFetchObject::continueReadBody(KHttpRequest *rq)
{
	if (rq->ctx->connection_upgrade) {
		return client->Read(rq, resultUpStreamReadBodyResult, bufferUpstreamRead);
	}
	if (!checkContinueReadBody(rq)) {
		return stage_rdata_end(rq,STREAM_WRITE_SUCCESS);
	}
	read_hup(rq);
	return client->Read(rq,resultUpStreamReadBodyResult, bufferUpstreamRead);
}
kev_result KAsyncFetchObject::readBody(KHttpRequest *rq)
{	
	if (us_buffer.used > 0) {
		return ParseBody(rq);
	}
	return continueReadBody(rq);
}
kev_result KAsyncFetchObject::handleReadBody(KHttpRequest *rq, int got)
{
	if (got <= 0) {
		if (rq->ctx->connection_upgrade) {
			return shutdown(rq);
		}
		parser_ctx.keep_alive_time_out = -1;
		return stage_rdata_end(rq, got == 0 ? STREAM_WRITE_SUCCESS : STREAM_WRITE_FAILED);
	}
	ks_write_success(&us_buffer, got);
	return ParseBody(rq);
}
kev_result KAsyncFetchObject::PushBodyResult(KHttpRequest *rq, StreamState result)
{
	switch(result) {
	case STREAM_WRITE_FAILED:
		if (rq->ctx->connection_upgrade) {
			return shutdown(rq);
		}
		return stage_rdata_end(rq, STREAM_WRITE_FAILED);
	case STREAM_WRITE_END:
		assert(rq->ctx->connection_upgrade == false);
		readBodyEnd(rq);
		return stage_rdata_end(rq, STREAM_WRITE_END);
	default:
		kev_result ret = try_send_request(rq);
		if (KEV_HANDLED(ret)) {
			return ret;
		}
		return continueReadBody(rq);
	}
}
kev_result KAsyncFetchObject::ParseBody(KHttpRequest *rq)
{
	char *data = us_buffer.buf;
	int len = us_buffer.used;
	StreamState ret = ParseBody(rq, &data, &len);
	ks_save_point(&us_buffer,data, len);
	return PushBodyResult(rq,ret);	
}
kev_result KAsyncFetchObject::handleHttp2ReadHead(KHttpRequest *rq,int got)
{
	if (got<0) {
		return handleUpstreamError(rq,STATUS_SERVER_ERROR,"Cann't Send head to remote server",got);
	}
	assert(got==0);
	badStage = BadStage_SendSuccess;
	return readHeadSuccess(rq);
}
kev_result KAsyncFetchObject::sendHeadSuccess(KHttpRequest *rq)
{
	buffer->destroy();
	if (rq->left_read!=0) {
		return readPost(rq);
	}
	return startReadHead(rq);
}
kev_result KAsyncFetchObject::startReadHead(KHttpRequest *rq)
{
	InitUpstreamBuffer();
	if (rq->ctx->connection_upgrade) {
		return readHeadSuccess(rq);
	}
	if (client->ReadHttpHeader(rq,resultUpstreamHttp2ReadHeader)) {
		return kev_ok;
	}
	client->SetNoDelay(false);
	return client->Read(rq,resultUpstreamReadHead, bufferUpstreamRead);
}
kev_result KAsyncFetchObject::handleSendHead(KHttpRequest *rq,int got)
{
	//printf("handleSendHead got=[%d]\n",got);
	if (got<=0) {
		return handleUpstreamError(rq,STATUS_SERVER_ERROR,"Cann't Send head to remote server",got);
	}
	badStage = BadStage_SendSuccess;
	if(buffer->readSuccess(got)){
		//continue send head
		return client->Write(rq,resultUpstreamSendHead,bufferUpstreamSendHead);
	}
	return sendHeadSuccess(rq);
}
kev_result KAsyncFetchObject::handleSendPost(KHttpRequest *rq,int got)
{
	if (got<=0) {
		return handleUpstreamError(rq,STATUS_SERVER_ERROR,"cann't send post data to remote server",got);
	}
	if (buffer->readSuccess(got)) {
		return client->Write(rq,resultUpstreamSendPost,bufferUpstreamSendPost);
	}
	//if (rq->ctx->connection_upgrade) {
		//client->Flush();
	//}
	buffer->destroy();
	//try to read post
	if (!rq->ctx->connection_upgrade && rq->left_read==0) {
		return startReadHead(rq);
	}
	return readPost(rq);	
}
kev_result KAsyncFetchObject::shutdown(KHttpRequest *rq)
{
	if (!rq->sink->IsLocked() &&
		!client->IsLocked() &&
		rq->ctx->write_hook==0) {
		return stageEndRequest(rq);
	}
	client->Shutdown();
	rq->sink->Shutdown();
	return kev_err;
}
kev_result KAsyncFetchObject::handleReadPost(KHttpRequest *rq,int got)
{
	//printf("handleReadPost got=[%d] protocol=[%d]\n",got,(int)rq->http_major);
	if (got == 0 && rq->left_read == -1 && !rq->ctx->connection_upgrade) {
		rq->left_read = 0;
		if (chunk_post) {
			BuildChunkHeader();
			return sendPost(rq);
		}
		client->WriteEnd();
		return startReadHead(rq);
	}
	if (got<=0) {
		if (rq->ctx->connection_upgrade) {
			return shutdown(rq);
		}
		return stageEndRequest(rq);
	}
	rq->AddUpFlow(got);
	if (!rq->ctx->connection_upgrade && rq->left_read!=-1) {
		rq->left_read -= got;
	}
	buffer->writeSuccess(got);
	if (chunk_post) {
		BuildChunkHeader();
	}
	return sendPost(rq);
}
void KAsyncFetchObject::BuildChunkHeader()
{
	int len = buffer->getLen();
	if (len == 0) {
		buffer->insert("0\r\n\r\n", 5);
		return;
	}
	char buf[32];
	int buf_len = snprintf(buf, sizeof(buf), "%x\r\n", len);
	buffer->insert(buf, buf_len);
	buffer->WSTR("\r\n");
}
kev_result KAsyncFetchObject::readPost(KHttpRequest *rq)
{
	return rq->Read(rq,resultUpstreamReadPost,bufferUpstreamReadPost);
}
kev_result KAsyncFetchObject::sendPost(KHttpRequest *rq)
{
	buildPost(rq);
	buffer->startRead();
	if (!rq->ctx->connection_upgrade) {
		read_hup(rq);
	}
	return client->Write(rq,resultUpstreamSendPost,bufferUpstreamSendPost);
}
kev_result KAsyncFetchObject::readHeadSuccess(KHttpRequest *rq)
{
	client->IsGood();
	if (rq->ctx->connection_upgrade) {
		rq->sink->SetNoDelay(true);
		client->SetNoDelay(true);
		int tmo = rq->sink->GetTimeOut();
		if (tmo < 5) {
			tmo = 5;
			rq->sink->SetTimeOut(tmo);
		}
		client->SetTimeOut(tmo);
		kev_result ret = rq->Read(rq, resultUpstreamReadPost, bufferUpstreamReadPost);
		if (!KEV_AVAILABLE(ret)) {
			return ret;
		}
	}
	return handleUpstreamRecvedHead(rq);
}
kev_result KAsyncFetchObject::handleReadHead(KHttpRequest *rq,int got)
{
	if (got<=0) {
		return handleUpstreamError(rq,STATUS_GATEWAY_TIMEOUT,"cann't recv head from remote server",got);
	}
	ks_write_success(&us_buffer, got);
	char *data = us_buffer.buf;
	int len = us_buffer.used;
	switch (ParseHeader(rq, &data, &len)) {
	case kgl_parse_finished:
		ks_save_point(&us_buffer, data, len);
		parser_ctx.EndParse(rq);
		return readHeadSuccess(rq);
	case kgl_parse_continue:
		if (parser.header_len > MAX_HTTP_HEAD_SIZE) {
			return handleUpstreamError(rq, STATUS_GATEWAY_TIMEOUT, "upstream protocol header size is too big", got);
		}
		ks_save_point(&us_buffer, data, len);
		return client->Read(rq, resultUpstreamReadHead, bufferUpstreamRead);
	case kgl_parse_want_read:
		//ajp协议会发送一个JK_AJP13_GET_BODY_CHUNK过来，此时要发送一个空的数据包过去
		return sendPost(rq);
	default:
		return handleUpstreamError(rq, STATUS_GATEWAY_TIMEOUT, "cann't parse upstream protocol", got);
	}
}
kgl_parse_result KAsyncFetchObject::ParseHeader(KHttpRequest *rq, char **data, int *len)
{
	khttp_parse_result rs;
	for (;;) {
		memset(&rs, 0, sizeof(rs));
		kgl_parse_result result = khttp_parse(&parser, data, len, &rs);
		switch (result) {
		case kgl_parse_error:
			return kgl_parse_error;
		case kgl_parse_continue:
			return kgl_parse_continue;
		case kgl_parse_success:
#if 0
			if (rs.is_first) {
				parser_ctx.StartParse(rq);
			}
#endif
			if (!parser_ctx.ParseHeader(rq, rs.attr, rs.attr_len, rs.val, rs.val_len, rs.request_line)) {
				return kgl_parse_error;
			}
			//printf("attr=[%s] val=[%s]\n", rs.attr, rs.val);
			break;
		case kgl_parse_finished:
			return kgl_parse_finished;
		}
	}
}
StreamState KAsyncFetchObject::ParseBody(KHttpRequest *rq, char **data, int *len)
{
	StreamState ret = PushBody(rq, *data, *len);
	*len = 0;
	return ret;
}
kev_result KAsyncFetchObject::handleUpstreamError(KHttpRequest *rq,int error,const char *msg,int last_got)
{
	kassert(client);
	int err = errno;
	parser_ctx.keep_alive_time_out = -1;
	if (!rq->ctx->read_huped) {
		if (client->IsNew()) {
			//new的才计算错误.
			client->IsBad(badStage);
		}
		SET(rq->flags, RQ_UPSTREAM_ERROR);
	}
	if (!rq->ctx->connection_connect_proxy && rq->ctx->connection_upgrade) {
		return shutdown(rq);
	}
	if (rq->ctx->read_huped) {
		//client broken
		return handleError(rq, error, msg);
	}	
	char *url = rq->url->getUrl();
	sockaddr_i *upstream_addr = client->GetAddr();
	char ips[MAXIPLEN];
	ksocket_sockaddr_ip(upstream_addr, ips, MAXIPLEN);
	klog(KLOG_WARNING, "rq=[%p] request=[%s %s] upstream=[%s:%d] self_port=[%d] error code=[%d],msg=[%s] errno=[%d %s],socket is %s,last_got=[%d].\n",
		rq,
		rq->getMethod(),
		url,
		ips,
		ksocket_addr_port(upstream_addr),
		client->GetSelfPort(),
		error,
		msg,
		err,
		strerror(err),
		(client->IsNew()?"new":"pool"),
		last_got
		);
	xfree(url);
	if (badStage != BadStage_SendSuccess && !client->IsNew()) {
		//use pool connectioin will try again
		return retryOpen(rq);
	}
	return handleError(rq,error,msg);
}
