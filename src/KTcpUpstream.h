#ifndef KTCPUPSTREAM_H
#define KTCPUPSTREAM_H
#include "KUpstream.h"
#include "kconnection.h"
class KTcpUpstream : public KUpstream {
public:
	KTcpUpstream(kconnection *cn)
	{
		this->cn = cn;
		if (this->cn == NULL) {
			this->cn = kconnection_new(NULL);
		}

	}
	~KTcpUpstream()
	{
		if (cn) {
			kconnection_destroy(cn);
		}
	}
	sockaddr_i *GetAddr()
	{
		if (cn) {
			return &cn->addr;
		}
		return NULL;
	}
	void EmptyConnection()
	{
		this->cn = NULL;
	}
	kconnection *GetConnection()
	{
		return this->cn;
	}
	void Shutdown()
	{
		selectable_shutdown(&cn->st);
	}
	void SetTimeOut(int tmo)
	{
		cn->st.tmo = tmo;
		cn->st.tmo_left = tmo;
	}
	bool IsLocked()
	{
		return TEST(cn->st.st_flags, STF_LOCK)>0;
	}
	bool BuildHttpHeader(KHttpRequest *rq, KWStream *s);
	void BindSelector(kselector *selector);
	kev_result Read(void *arg, result_callback result, buffer_callback buffer);
	kev_result Write(void *arg, result_callback result, buffer_callback buffer);
	kev_result Connect(void *arg, result_callback result);
	void Gc(int life_time,time_t last_recv_time);
	void OnPushContainer();
	void Destroy()
	{
		delete this;
	}
private:
	kconnection *cn;

};
#endif
