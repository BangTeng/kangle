#ifndef KSIMULATEREQUEST_H
#define KSIMULATEREQUEST_H
#include "global.h"
#include "ksocket.h"
#include "KHttpHeader.h"
#include "ksapi.h"
#include "kconnection.h"
#include "KStream.h"
#include "KFileName.h"
#include "KSink.h"
#ifdef ENABLE_SIMULATE_HTTP
int asyncHttpRequest(kgl_async_http *ctx);
class KFileStream : public KWStream {
public:
	bool open(const char *str)
	{
		return fp.open(str, fileWrite);
	}
	int write(const char *buf, int len)
	{
		return fp.write(buf, len);
	}
	StreamState write_end() {		
		if (last_modified > 0) {
			if (!kfutime(fp.getHandle(), last_modified)) {
				//printf("update modifi time=[%x] error\n", (unsigned)last_modified);
			}
		}
		fp.close();
		return STREAM_WRITE_SUCCESS;
	}
	time_t last_modified;
private:
	KFile fp;
};
class KSimulateSink : public KSink
{
public:
	KSimulateSink();
	~KSimulateSink();

	bool SetTransferChunked()
	{
		return false;
	}
	bool ResponseStatus(uint16_t status_code)
	{
		this->status_code = status_code;
		return true;
	}
	bool ResponseHeader(const char *name, int name_len, const char *val, int val_len)
	{
		KHttpHeader *header = new_http_header(c->pool, name, name_len, val, val_len);
		if (header) {
			header_manager.Append(header);
		}
		return true;
	}
	bool ResponseConnection(const char *val, int val_len)
	{
		return false;
	}
	//返回头长度,-1表示出错
	int StartResponseBody(KHttpRequest *rq, int64_t body_size)
	{
		if (header) {
			header(arg, status_code, header_manager.header);
		}
		return 0;
	}
	bool IsLocked()
	{
		return false;
	}
	kev_result Write(void *arg, result_callback result, buffer_callback buffer)
	{
		WSABUF buf;
		int bc = buffer(arg, &buf, 1);
		if (bc != 1) {
			return result(arg, -1);
		}
		int got = Write(&buf, 1);
		return result(arg, got);
	}
	kev_result Read(void *arg, result_callback result, buffer_callback buffer)
	{
		WSABUF buf;
		int bc = buffer(arg, &buf, 1);
		if (bc != 1) {
			return result(arg, -1);
		}
		int got = Read((char *)buf.iov_base, buf.iov_len);
		return result(arg, got);
	}
	bool ReadHup(void *arg, result_callback result)
	{
		return false;
	}
	void RemoveReadHup()
	{

	}
	int Read(char *buf, int len)
	{
		if (post) {
			return post(arg, buf, len);
		}
		return 0;
	}
	bool HasHeaderDataToSend()
	{
		return false;
	}
	int Write(LPWSABUF buf, int bc)
	{
		if (0 == body(arg, (char *)buf[0].iov_base, buf[0].iov_len)) {
			return buf[0].iov_len;
		}
		return -1;

	}
	sockaddr_i *GetAddr()
	{
		return &c->addr;
	}
	bool GetSelfAddr(sockaddr_i *addr)
	{
		kgl_memcpy(addr, &c->addr, sizeof(sockaddr_i));
		return true;
	}
	void EndRequest(KHttpRequest *rq);
	void AddSync()
	{

	}
	void RemoveSync()
	{

	}
	void Shutdown()
	{

	}
	kconnection *GetConnection()
	{
		return c;
	}
	void SetTimeOut(int tmo_count)
	{

	}
	int GetTimeOut()
	{
		return c->st.tmo;
	}
	void Flush()
	{

	}
	void SetWriteLimit(bool limit_flag)
	{

	}
	void SetReadLimit(bool limit_flag)
	{

	}
	http_post_hook post;
	http_header_hook header;
	http_body_hook body;
	KHttpHeaderManager header_manager;
	char *host;
	unsigned short port;
	int life_time;
	int exptected_done;
	void *arg;
	kconnection *c;
	uint16_t status_code;
protected:
	void SetReadDelay(bool delay_flag)
	{

	}
	void SetWriteDelay(bool delay_flag)
	{

	}

};
void async_download(const char *url, const char *file, result_callback cb, void *arg);
bool test_simulate_request();
#endif
#endif
