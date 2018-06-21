/*
 * KPoolableStream.h
 *
 *  Created on: 2010-8-18
 *      Author: keengo
 */

#ifndef KUPSTREAMSELECTABLE_H
#define KUPSTREAMSELECTABLE_H
#include <time.h>
#include "global.h"
#include "KStream.h"
#include "KSocket.h"
#include "KConnectionSelectable.h"
#include "KSSLSocket.h"
#define KPoolableStream KStream
enum BadStage
{
	/* BadStage_Connect BadStage_TrySend  */
	BadStage_Connect,
	BadStage_TrySend,
	/* BadStage_SendSuccess  */
	BadStage_SendSuccess,
};
class KPoolableSocketContainer;

class KHttpRequest;
class KHttpProtocolParser;
class KHttpObjectParserHook;
/*
 */
class KUpstreamSelectable : public KConnectionSelectable
{
public:
	KUpstreamSelectable(KClientSocket *sockfd);

	bool isNew() {
		return expireTime == 0;
	}
	int getLifeTime();
	int get_delay_read()
	{
		return this->delay_read_msec;
	}
	void set_delay_read(int delay_read_msec)
	{
		this->delay_read_msec = delay_read_msec;
	}
	void isBad(BadStage stage);
	void isGood();
	void connect(KHttpRequest *rq,resultEvent result);
	bool is_upstream_event(uint16_t flag);
	bool is_upstream_locked();
	void upstream_read(KHttpRequest *rq,resultEvent result,bufferEvent buffer);
	void upstream_write(KHttpRequest *rq,resultEvent result,bufferEvent buffer);

	/*
	 *
	 * lifeTime = -1 close the connection
	 * lifeTime = 0  use the default lifeTime
	 */
	void gc(int lifeTime);
	void destroy();
	void upstream_shutdown();
	void prepare_parser(KHttpRequest *rq);

	time_t expireTime;
	//int use_count;	
	/*
	 */
	KPoolableSocketContainer *container;
	friend class KPoolableSocketContainer;
	KHttpProtocolParser *parser;
	KHttpObjectParserHook *hook;
	
protected:
	~KUpstreamSelectable();
	int delay_read_msec;
};
#endif /* KPOOLABLESTREAM_H_ */
