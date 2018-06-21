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

struct kgl_upstream_delay_io
{
	KUpstreamSelectable *us;
	KHttpRequest *rq;
	bufferEvent buffer;
	resultEvent result;
};
void upstream_delay_read(void *arg,int got)
{
	kgl_upstream_delay_io *io = (kgl_upstream_delay_io *)arg;
	io->us->upstream_read(io->rq, io->result, io->buffer);
	delete io;
}
KUpstreamSelectable::KUpstreamSelectable(KClientSocket *sockfd) : KConnectionSelectable(sockfd)
{
	expireTime = 0;
	container = NULL;
	parser = NULL;
	hook = NULL;
	delay_read_msec = 0;
	
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
	assert(TEST(st_flags, STF_LOCK) == 0);
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
	if (container == NULL) {
		destroy();
		return;
	}
	
	if (selector) {
		assert(queue.next == NULL);
	}
#ifndef _WIN32
	if (selector) {
		selector->removeSocket(this);
		this->selector = NULL;
	}
	assert(TEST(st_flags,STF_READ|STF_WRITE|STF_REV|STF_WEV)==0);
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
	
	//selector->addList(this, KGL_LIST_CONNECT);
	if (!this->selector->connect(this,result,rq)) {
		result(rq,-1);
	}
}

void KUpstreamSelectable::upstream_read(KHttpRequest *rq,resultEvent result,bufferEvent buffer)
{
	int msec = this->delay_read_msec;
	if (msec > 0) {
		this->delay_read_msec = 0;
		kgl_upstream_delay_io *io = new kgl_upstream_delay_io;
		io->us = this;
		io->rq = rq;
		io->result = result;
		io->buffer = buffer;
		selector->add_timer(upstream_delay_read, io, msec,NULL);
		return;
	}
	
	async_read(rq,result,buffer);
}
void KUpstreamSelectable::upstream_write(KHttpRequest *rq,resultEvent result,bufferEvent buffer)
{
	
	async_write(rq,result,buffer);
}
bool KUpstreamSelectable::is_upstream_event(uint16_t flag)
{
#ifdef ENABLE_UPSTREAM_HTTP2
	if (http2_ctx) {
		if (TEST(flag, STF_REVENT) && http2_ctx->read_wait != NULL) {
			return true;
		}
		if (TEST(flag, STF_WEVENT) && http2_ctx->write_wait != NULL) {
			return true;
		}
		return false;
	}
#endif
	flag = (flag & STF_EVENT);
	return TEST(st_flags, flag) > 0;
}
bool KUpstreamSelectable::is_upstream_locked()
{
#ifdef ENABLE_UPSTREAM_HTTP2
	if (http2_ctx) {
		if (http2_ctx->read_wait != NULL) {
			return true;
}
		if (http2_ctx->write_wait != NULL && http2_ctx->write_wait->buffer != NULL) {
			return true;
		}
		return false;
	}
#endif
	return TEST(st_flags, STF_LOCK) > 0;
}

