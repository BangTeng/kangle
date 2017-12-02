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
	/* BadStage_Connect BadStage_TrySend �������� */
	BadStage_Connect,
	BadStage_TrySend,
	/* BadStage_SendSuccess ���������� */
	BadStage_SendSuccess,
};
class KPoolableSocketContainer;

class KHttpRequest;
class KHttpProtocolParser;
class KHttpObjectParserHook;
/*
 * �ɱ��ظ�ʹ�õ����ӡ�
 */
class KUpstreamSelectable : public KConnectionSelectable
{
public:
	KUpstreamSelectable(KClientSocket *sockfd);
	
	/*
	 * �����Ƿ����µ�
	 */
	bool isNew() {
		return expireTime == 0;
	}
	int getLifeTime();
	void isBad(BadStage stage);
	void isGood();
	void connect(KHttpRequest *rq,resultEvent result);
	void upstream_remove();
	/* �첽�� */
	bool upstream_read(KHttpRequest *rq,resultEvent result,bufferEvent buffer);
	/* �첽д */
	void upstream_write(KHttpRequest *rq,resultEvent result,bufferEvent buffer);

	/*
	 * ɾ�����ӣ�������Ƿ�������ӳ��С�
	 * lifeTime = -1 close the connection
	 * lifeTime = 0  use the default lifeTime
	 */
	void gc(int lifeTime);
	void destroy();
	void upstream_shutdown();
	void prepare_parser(KHttpRequest *rq);
	/*
	 * ���ӹ���ʱ��
	 */
	INT64 expireTime;
	int use_count;
	/*
	 * ���������ӳ�����
	 */
	KPoolableSocketContainer *container;
	friend class KPoolableSocketContainer;
	KHttpProtocolParser *parser;
	KHttpObjectParserHook *hook;
	
protected:
	~KUpstreamSelectable();	
};
#endif /* KPOOLABLESTREAM_H_ */
