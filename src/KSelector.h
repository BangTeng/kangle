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
#include "ksapi.h"
#include "forwin32.h"
#include "KList.h"
#include "katom.h"
#include "extern.h"
#define STAGE_OP_TIMER             0
#define STAGE_OP_PM_UREAD          12
#define STAGE_OP_PM_UWRITE         13
#define STAGE_OP_PM_READ           14
#define STAGE_OP_PM_WRITE          15
#define STAGE_OP_LISTEN            16
#define STAGE_OP_ASYNC_READ        17
#define STAGE_OP_TRANSMIT          17
//新的事件,以后上面的废除
#define STAGE_OP_NEW_READ          23
#define STAGE_OP_NEW_WRITE         24
#define IS_SECOND_OPERATOR(op)     (op==18)

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
class KHttpRequest;
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
class KSelector {
public:
	KSelector();
	virtual ~KSelector();
	//	void setMaxRequest(int maxRequest);
	//	bool addRequest(KClientSocket *socket,int model);
	virtual const char *getName() = 0;
	//void sslAccept(KHttpRequest *rq);
	/*
	其它线程调用，加list和addSocket为原子操作
	*/
	//void addRequest(KHttpRequest *rq,int list,int op);
	//void removeRequest(KHttpRequest *rq);
	virtual void bindSelectable(KSelectable *st);
	bool startSelect();
	void selectThread();
	bool isSameThread()
	{
		return pthread_self()==thread_id;
	}
	friend class KHttpRequest;
	//update time
	bool utm;
	int tmo_msec;
	int sid;
	int count;
	//超时时间，单位msec
	int timeout[KGL_LIST_BLOCK];
	void callback(KSelectable *st, resultEvent func, void *arg);
	void addTimer(KSelectable *rq, timer_func func, void *arg, int msec);
	void addTimer(KSelectable *rq, resultEvent func, void *arg, int msec);
	virtual bool next(KSelectable *st,resultEvent result,void *arg) = 0;
	virtual bool listen(KServer *st,resultEvent result) {
		return false;
	}
	virtual void removeListenSocket(KSelectable *st) {
		
	}
	unsigned getConnection(std::stringstream &s,const char *vh_name,bool translate, volatile uint32_t *total_count);
	void adjustTime(INT64 t);
	/*
	增加事件
	*/
	//virtual bool addSocket(KSelectable *st,int op) = 0;	
	//void addList(KHttpRequest *rq,int list);
	//void removeList(KHttpRequest *rq);
	void removeList(KSelectable *st);
	void addList(KSelectable *rq, int list);
	friend class KSelectable;
	friend class KConnectionSelectable;
	friend class KServer;
	friend class KUpstreamSelectable;
	friend class KHttp2;
	friend class KHttp2ConnectionPreface;
	friend class KSSLBIO;
protected:
	friend class KSelectorManager;
	friend class KAsyncFetchObject;
	KMutex listLock;
	kgl_list list[KGL_LIST_BLOCK];
	virtual void select()=0;
	void checkTimeOut();
	int model;
	void internelAddRequest(KHttpRequest *rq);
#ifdef MALLOCDEBUG
	bool can_close()
	{
		return quit_program_flag > PROGRAM_QUIT_IMMEDIATE && count == 0 && blockList.rb_node == NULL;
	}
#endif
private:
	void getConnectionTr(KHttpRequest *rq, std::stringstream &s,time_t now_time,bool translate);
	virtual void removeSocket(KSelectable *st) = 0;
	virtual bool read(KSelectable *st,resultEvent result,bufferEvent buffer,void *arg) = 0;
	virtual bool read_hup(KSelectable *st,resultEvent result,bufferEvent buffer,void *arg)
	{
		return false;
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
