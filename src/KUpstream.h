/*
 * KPoolableStream.h
 *
 *  Created on: 2010-8-18
 *      Author: keengo
 */

#ifndef KUPSTREAM_H
#define KUPSTREAM_H
#include <time.h>
#include "global.h"
#include "kselector.h"
#include "ksocket.h"
#include "kconnection.h"
#include "kmalloc.h"

enum BadStage
{
	/* BadStage_Connect BadStage_TrySend  */
	BadStage_Connect,
	BadStage_TrySend,
	/* BadStage_SendSuccess  */
	BadStage_SendSuccess,
};
class KWStream;
class KPoolableSocketContainer;
class KHttpRequest;
class KUpstream
{
public:
	KUpstream()
	{
		expire_time = 0;
		container = NULL;
	}
	virtual void SetDelay()
	{

	}
	virtual void SetNoDelay(bool forever)
	{
	}
	virtual kev_result Connect(void *arg, result_callback result)
	{
		return result(arg, -1);
	}
	virtual bool BuildHttpHeader(KHttpRequest *rq, KWStream *s)
	{
		return false;
	}
	virtual bool ReadHttpHeader(void *arg,result_callback result)
	{
		return false;
	}
	virtual kgl_pool_t *GetPool()
	{
		return NULL;
	}
	virtual kconnection *GetConnection() = 0;
	virtual void WriteEnd()
	{

	}
	virtual kev_result Read(void *arg, result_callback result, buffer_callback buffer) = 0;
	virtual kev_result Write(void *arg, result_callback result, buffer_callback buffer) = 0;
	virtual void Shutdown() = 0;
	virtual void Destroy() = 0;
	virtual bool IsLocked() = 0;
	virtual bool IsMultiStream()
	{
		return false;
	}
	virtual void SetTimeOut(int tmo)
	{
	}
	virtual void BindSelector(kselector *selector)
	{

	}
	virtual void OnPushContainer()
	{

	}
	virtual KUpstream *NewStream(KHttpRequest *rq)
	{
		return NULL;
	}
	virtual bool IsNew() {
		return expire_time == 0;
	}
	virtual int GetLifeTime();
	virtual void IsGood();	
	virtual void IsBad(BadStage stage);
	virtual sockaddr_i *GetAddr() = 0;
	bool GetSelfAddr(sockaddr_i *addr)
	{
		kconnection *cn = GetConnection();
		if (cn == NULL) {
			return false;
		}
		return 0 == kconnection_self_addr(cn, addr);
	}
	uint16_t GetSelfPort()
	{
		sockaddr_i addr;
		if (!GetSelfAddr(&addr)) {
			return 0;
		}
		return ksocket_addr_port(&addr);
	}
	virtual void Gc(int life_time,time_t base_time) = 0;
	friend class KPoolableSocketContainer;
	time_t expire_time;
	KPoolableSocketContainer *container;
protected:
	virtual ~KUpstream();
};
#define KPoolableUpstream KUpstream
#endif /* KPOOLABLESTREAM_H_ */
