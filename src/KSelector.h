/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */
#ifndef KSELECTOR_H_
#define KSELECTOR_H_
#include "KSocket.h"
#include "KMutex.h"
#include "log.h"
#include "rbtree.h"
#include "KRequestList.h"
#include "KFile.h"
#include "ksapi.h"
#include "forwin32.h"
#include "KList.h"
#include "katom.h"
#include "extern.h"

#ifdef _WIN32
#define ASSERT_SOCKFD(a)        assert(a>=0)
#else
#define ASSERT_SOCKFD(a)        assert(a>=0 && a<81920)
#endif
typedef void (WINAPI *timer_func)(void *arg);

typedef void (*resultEvent)(void *arg,int got);
typedef void (*bufferEvent)(void *arg,LPWSABUF buf,int &bufCount);
enum ReadState {
        READ_FAILED, READ_CONTINUE, READ_SUCCESS
};
enum WriteState {
        WRITE_FAILED, WRITE_CONTINUE, WRITE_SUCCESS
};
#define OP_READ  0
#define OP_WRITE 1
#define STATE_UNKNOW   0
#define STATE_IDLE     1
#define STATE_SEND     2
#define STATE_RECV     3
#define STATE_QUEUE    4

FUNC_TYPE FUNC_CALL manageWorkThread(void *param);
FUNC_TYPE FUNC_CALL httpWorkThread(void *param);
FUNC_TYPE FUNC_CALL httpsWorkThread(void *param);
FUNC_TYPE FUNC_CALL oneWorkoneThread(void *param);

FUNC_TYPE FUNC_CALL stage_sync(void *param);
FUNC_TYPE FUNC_CALL stage_rdata(void *param);
#ifdef _WIN32
FUNC_TYPE FUNC_CALL stageRequest(void *param) ;
#endif
class KSelectable;
class KServer;
class KServerSelectable;
class KHttpRequest;
class KAsyncFile;
void stage_prepare(KHttpRequest *rq);
void handleAccept(KSelectable *st,int got);
//void handleRequestRead(KSelectable *st,int got);
void resultRequestRead(void *arg,int got);
void bufferRequestRead(void *arg,LPWSABUF buf,int &bufCount);

void handleRequestWrite(KSelectable *st,int got);
void handleRequestTempFileWrite(KSelectable *st,int got);
void handleStartRequest(KHttpRequest *rq,int got);

void log_access(KHttpRequest *rq);
int checkHaveNextRequest(KHttpRequest *rq);
void stageEndRequest(KHttpRequest *rq);
#ifdef ENABLE_TF_EXCHANGE
void stageTempFileWriteEnd(KHttpRequest *rq);
#endif
typedef void(*aio_callback)(KAsyncFile *fp, void *arg, char *buf, int length);
class KSelector {
public:
	KSelector();
	virtual ~KSelector();
	virtual const char *getName() = 0;
	virtual void bindSelectable(KSelectable *st);
	bool startSelect();
	void selectThread();
	bool is_same_thread()
	{
		return pthread_self()==thread_id;
	}
	friend class KHttpRequest;
	//update time
	bool utm;
	int tmo_msec;
	int sid;
	int count;
	//msec
	int timeout[KGL_LIST_BLOCK];
	void add_timer(resultEvent func, void *arg, int msec, KSelectable *st);
	virtual bool next(resultEvent result,void *arg,int got) = 0;
	bool next(resultEvent result, void *arg)
	{
		return next(result, arg, 0);
	}
	virtual bool listen(KServerSelectable *st,resultEvent result) {
		return false;
	}
	unsigned getConnection(std::stringstream &s,const char *vh_name,bool translate, volatile uint32_t *total_count);
	void adjustTime(INT64 t);
	virtual bool aio_read(KAsyncFile *file,char *buf,INT64 offset,int length,aio_callback cb,void *arg)
	{
		return false;
	}
	virtual bool aio_write(KAsyncFile *file, char *buf, INT64 offset, int length, aio_callback cb, void *arg)
	{
		return false;
	}
	virtual KAsyncFile *aio_open(KFile *fp)
	{
		return NULL;
	}
	friend class KSelectable;
	friend class KConnectionSelectable;
	friend class KServerSelectable;
	friend class KUpstreamSelectable;
	friend class KHttp2;
	friend class KHttp2ConnectionPreface;
	friend class KSSLBIO;
protected:
	friend class KSelectorManager;
	friend class KAsyncFetchObject;
	//KMutex listLock;
	kgl_list list[KGL_LIST_BLOCK];

	virtual void select()=0;
	void checkTimeOut();
	void remove_list(KSelectable *st);
	void add_list(KSelectable *rq, int list);
	int model;
	void internelAddRequest(KHttpRequest *rq);
#ifdef MALLOCDEBUG
	bool can_close()
	{
		return quit_program_flag > PROGRAM_QUIT_IMMEDIATE && count == 0 && blockList.rb_node == NULL && closed_flag;
	}
	void close()
	{
		closed_flag = true;
	}
	bool closed_flag;
#endif
private:
	void getConnectionTr(KHttpRequest *rq, std::stringstream &s,time_t now_time,bool translate);
	virtual void removeSocket(KSelectable *st) = 0;
	virtual bool read(KSelectable *st,resultEvent result,bufferEvent buffer,void *arg) = 0;
	virtual bool read_hup(KSelectable *st,resultEvent result,bufferEvent buffer,void *arg)
	{
		return false;
	}
	virtual void remove_read_hup(KSelectable *st)
	{

	}
	virtual bool write(KSelectable *st,resultEvent result,bufferEvent buffer,void *arg) = 0;
	//先调用halfconnect,再调用connect事件.
	virtual bool connect(KSelectable *st,resultEvent result,void *arg) = 0;
	rb_root blockList;
	rb_node *blockBeginNode;
	pthread_t thread_id;
};
extern char serverData[];
#endif /*KSELECTOR_H_*/
