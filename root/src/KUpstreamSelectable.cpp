/*
 * KPoolableStream.cpp
 *
 *  Created on: 2010-8-18
 *      Author: keengo
 */

#include "KUpstreamSelectable.h"
#include "KPoolableSocketContainer.h"
#include "log.h"
#include "KConnectionSelectable.h"
#include "KHttpProxyFetchObject.h"
#include "KHttpRequest.h"
#include "KSelector.h"

KUpstreamSelectable::KUpstreamSelectable(KClientSocket *sockfd) : KConnectionSelectable(sockfd)
{
	expireTime = 0;
	container = NULL;
	parser = NULL;
	hook = NULL;
	
}
void KUpstreamSelectable::prepare_parser(KHttpRequest *rq)
{
	if (parser == NULL) {
		parser = new KHttpProtocolParser;
	}
	if (hook == NULL) {
		hook = new KHttpObjectParserHook;
	}
	parser->restart();
	hook->init(rq->ctx->obj, rq);
}
KUpstreamSelectable::~KUpstreamSelectable() {
	if (container) {
		container->unbind(this);
	}
	if (parser) {
		delete parser;
	}
	if (hook) {
		delete hook;
	}
	
}
void KUpstreamSelectable::destroy()
{
	
	real_destroy();
}
void KUpstreamSelectable::gc(int lifeTime) {
	if (parser) {
		delete parser;
		parser = NULL;
	}
	if (hook) {
		delete hook;
		hook = NULL;
	}
	if (selector) {
		selector->removeList(this);
	}
	if (container == NULL) {
		destroy();
		return;
	}
	
#ifndef _WIN32
	if (selector) {
		selector->removeSocket(this);
		this->selector = NULL;
	}
	/* linux下确保 upstream在主线程上 */
	assert(TEST(st_flags,STF_READ|STF_WRITE|STF_EV)==0);
#endif
	container->gcSocket(this,lifeTime);
}
void KUpstreamSelectable::upstream_shutdown()
{
	
	socket->shutdown(SHUT_RDWR);
}
void KUpstreamSelectable::isBad(BadStage stage)
{
	if (container) {
		container->isBad(this,stage);
	}
}
void KUpstreamSelectable::isGood()
{
	if (container) {
		container->isGood(this);
	}
}
int KUpstreamSelectable::getLifeTime()
{
	if (container) {
		return container->getLifeTime();
	}
	return 0;
}
void KUpstreamSelectable::connect(KHttpRequest *rq,resultEvent result)
{
	
	selector->addList(this, KGL_LIST_CONNECT);
	if (!this->selector->connect(this,result,rq)) {
		result(rq,-1);
	}
}
void KUpstreamSelectable::upstream_remove()
{
	
	internalRemoveRequest(false);
}
/* 异步读 */
bool KUpstreamSelectable::upstream_read(KHttpRequest *rq,resultEvent result,bufferEvent buffer)
{
	
	return asyncRead(rq,result,buffer);
}
/* 异步写 */
void KUpstreamSelectable::upstream_write(KHttpRequest *rq,resultEvent result,bufferEvent buffer)
{
	
	asyncWrite(rq,result,buffer);
}

