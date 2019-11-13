#ifndef KHTTPSINK_H_99
#define KHTTPSINK_H_99
#include "KSink.h"
#include "kconnection.h"
#include "KHttpParser.h"
#include "KBuffer.h"
#include "KConfig.h"
#include "KResponseContext.h"
#include "KHttpHeader.h"
#include "KDechunkContext.h"
//处理http1协议的
int buffer_read_http_sink(void *arg, LPWSABUF buf, int bufCount);

class KHttpSink : public KSink {
public:
	KHttpSink(kconnection *c);
	~KHttpSink();
	bool ResponseStatus(uint16_t status_code);
	bool ResponseHeader(const char *name, int name_len, const char *val, int val_len);
	bool ResponseConnection(const char *val, int val_len) {
		return ResponseHeader(kgl_expand_string("Connection"), val, val_len);
	}
	void SetChunkRawRead()
	{
		kassert(dechunk);
		if (dechunk) {
			dechunk->SetChunkRawRead();
		}
	}
	bool HasHeaderDataToSend()
	{
		return (rc && rc->GetLen() > 0);
	}
	int GetReadPending()
	{
		return buffer.used;
	}
	void StartHeader(KHttpRequest *rq);
	int StartResponseBody(bool sync, int64_t body_size);
	bool IsLocked();
	kev_result Write(void *arg, result_callback result, buffer_callback buffer);
	kev_result Read(void *arg, result_callback result, buffer_callback buffer);
	int Read(char *buf, int len);
	int Write(LPWSABUF buf, int bc);
	bool ReadHup(void *arg, result_callback result)
	{
		return selectable_readhup(&cn->st, result, arg);
	}
	void RemoveReadHup()
	{
		selectable_remove_readhup(&cn->st);
	}
	void AddSync()
	{
		selectable_add_sync(&cn->st);
	}
	void RemoveSync()
	{
		selectable_remove_sync(&cn->st);
	}
	void Flush()
	{
		ksocket_no_delay(cn->st.fd);
		ksocket_delay(cn->st.fd);
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
	int GetTimeOut()
	{
		return cn->st.tmo;
	}
	void EndRequest(KHttpRequest *rq);
	ks_buffer buffer;
	kev_result ReadHeader(KHttpRequest *rq);
	kev_result Parse(KHttpRequest *rq);
	sockaddr_i *GetAddr() {
		return &cn->addr;
	}
	bool GetSelfAddr(sockaddr_i *addr) {	
		return 0==kconnection_self_addr(cn,addr);
	}
	KResponseContext *rc;
	kconnection *cn;
	kev_result ResultResponseContext(int got);
	kconnection *GetConnection()
	{
		return cn;
	}
	KDechunkContext *GetDechunkContext()
	{
		return dechunk;
	}
#ifdef KSOCKET_SSL
	SSL *GetSSL() {
		if (cn->st.ssl == NULL) {
			return NULL;
		}
		return cn->st.ssl->ssl;
	}
#endif
	void SkipPost(KHttpRequest *rq);
	void StartPipeLine(KHttpRequest *rq);
protected:
	KDechunkContext *dechunk;
	khttp_parser parser;
};
#endif

