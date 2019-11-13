#ifndef KTSUPSTREAM_H
#define KTSUPSTREAM_H
#include "KUpstream.h"
//thread safe upstream
#define TS_UPSTREAM_BUFFER_COUNT 16
class KTsUpstreamStack {
public:
	KTsUpstreamStack()
	{
		memset(this, 0, sizeof(KTsUpstreamStack));
	}
	result_callback result;	
	WSABUF buf[TS_UPSTREAM_BUFFER_COUNT];
	int bc;
	void *arg;
	int buffer(LPWSABUF buf, int bc)
	{
		int copy_bc = MIN(bc, this->bc);
		memcpy(buf, this->buf, copy_bc * sizeof(WSABUF));
		return copy_bc;
	}
} ;

class KTsUpstream : public KUpstream {
public:
	KTsUpstream(kselector *main_selector,KUpstream *us)
	{
		this->main_selector = main_selector;
		this->us = us;
		flags = 0;
	}
	~KTsUpstream()
	{
		kassert(us == NULL);
	}
	void SetDelay()
	{
		us->SetDelay();
	}
	void SetNoDelay()
	{
		us->SetNoDelay();
	}
	kconnection *GetConnection()
	{
		return us->GetConnection();
	}
	bool BuildHttpHeader(KHttpRequest *rq, KWStream *s)
	{
		return us->BuildHttpHeader(rq, s);
	}
	void SetTimeOut(int tmo)
	{
		return us->SetTimeOut(tmo);
	}
	bool ReadHttpHeader(void *arg,result_callback result);
	kev_result Connect(void *arg, result_callback result);
	kev_result Read(void *arg, result_callback result, buffer_callback buffer);
	kev_result Write(void *arg, result_callback result, buffer_callback buffer);
	kev_result NextReadHeader();
	kev_result NextRead();
	kev_result NextWrite();
	bool IsMultiStream()
	{
		return us->IsMultiStream();
	}
	bool IsNew() {
		return us->IsNew();
	}
	int GetLifeTime()
	{
		return us->GetLifeTime();
	}
	void IsGood()
	{
		return us->IsGood();
	}
	void IsBad(BadStage stage)
	{
		return us->IsBad(stage);
	}
	void WriteEnd();
	void Shutdown();
	void Destroy();
	bool IsLocked();
	bool GetSelfAddr(sockaddr_i *addr)
	{

	}
	sockaddr_i *GetAddr()
	{
		return us->GetAddr();
	}
	int GetPoolSid()
	{
		return 0;
	}
	kgl_pool_t *GetPool()
	{
		return us->GetPool();
	}
	void Gc(int life_time,time_t last_recv_time);
	KTsUpstreamStack stack[2];
	kselector *main_selector;
	KUpstream *us;
	union {
		struct {
			uint8_t shutdown : 1;
			uint8_t read_lock : 1;
			uint8_t write_lock : 1;
			uint8_t write_end : 1;
		};
		uint8_t flags;
	};
private:
};
#endif
