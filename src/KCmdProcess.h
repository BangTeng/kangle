/*
 * KCmdProcess.h
 *
 *  Created on: 2010-10-24
 *      Author: keengo
 */

#ifndef KCMDPROCESS_H_
#define KCMDPROCESS_H_

#include "KVirtualHostProcess.h"
#include "klist.h"
#include "KListenPipeStream.h"
#include "kselector_manager.h"
#include "KTcpUpstream.h"
/*
多线程命令进程
*/
class KCmdProcess: public KVirtualHostProcess {
public:
	KCmdProcess();
	~KCmdProcess();
	KTcpUpstream *poweron(KVirtualHost *vh,KExtendProgram *rd,bool &success);
	void getProcessInfo(const USER_T &user, const std::string &name,
			std::stringstream &s,int &count)
	{
		stLock.Lock();
		if (st) {
			count++;
			::getProcessInfo(user,name,&st->process,this,s);
		}
		stLock.Unlock();
	}
	bool killProcess(int pid) {
		stLock.Lock();
		if (st) {
			delete st;
			st = NULL;
		}
		status = VProcess_Close;
		stLock.Unlock();
		return true;
	}
	
protected:
	bool isProcessActive()
	{
		bool result = false;
		stLock.Lock();		
		if(st){
			result = st->process.isActive();
		}
		stLock.Unlock();
		return result;
	}
	KMutex stLock;
	KListenPipeStream *st;
};
class KSingleListenPipeStream;
//多进程命令扩展
class KMPCmdProcess: public KVirtualHostProcess {
public:
	KMPCmdProcess();
	~KMPCmdProcess();
	kev_result handleRequest(KHttpRequest *rq,KExtendProgram *rd);
	void getProcessInfo(const USER_T &user, const std::string &name,std::stringstream &s,int &count);
	bool killProcess(int pid);
	KTcpUpstream *poweron(KVirtualHost *vh,KExtendProgram *erd,bool &success);
	void gcProcess(KSingleListenPipeStream *st);
	bool isMultiProcess()
	{
		return true;
	}
	
	bool canDestroy(time_t nowTime);
private:
	KSingleListenPipeStream *freeProcessList;
	KSingleListenPipeStream *busyProcessList;
	KMutex stLock;
	KMutex cmdLock;
};
class KSingleListenPipeStream : public KListenPipeStream,public KPoolableSocketContainer
{
public:
	KSingleListenPipeStream()
	{
		socket = NULL;
		lastActive = kgl_current_sec;
	}
	~KSingleListenPipeStream()
	{
		if (socket) {
			socket->Destroy();
		}
		unlink_unix();
	}
	KUpstream *getConnection(KHttpRequest *rq,bool &isHalf)
	{
		lastActive = kgl_current_sec;
		KTcpUpstream *st = socket;
		if(st == NULL) {
			st = static_cast<KTcpUpstream *>(KPoolableSocketContainer::getPoolSocket(rq));
			if (st) {
				isHalf = false;
				return st;
			}
			isHalf = true;
			kconnection *cn = kconnection_new(&addr);
#ifdef KSOCKET_UNIX
			if (!unix_path.empty()) {
				struct sockaddr_un un_addr;
				ksocket_unix_addr(unix_path.c_str(),&un_addr);
				SOCKET fd = ksocket_half_connect((sockaddr_i *)&un_addr,NULL,0);
				if (!ksocket_opened(fd)) {
					kconnection_destroy(cn);
					return NULL;
				}
				cn->st.fd = fd;
			} else
#endif
			kconnection_half_connect(cn, NULL, 0);
			st = new KTcpUpstream(cn);
		} else {	
			isHalf = false;
			socket = NULL;
		}
		if (st) {
			bind(st);
		}
		return st;
	}
	void gcSocket(KPoolableUpstream *st,int lifeTime,time_t last_recv_time)
	{
#if 0
#ifdef _WIN32
		if (get_selector_count() == 1) {
			//windows下只有一个selector时才使用长连接
			KPoolableSocketContainer::gcSocket(st, lifeTime, last_recv_time);
		} else {
			st->Destroy();
		}
#else
		KPoolableSocketContainer::gcSocket(st, lifeTime, last_recv_time);
#endif
#endif
		//使用了KTsUpstream后，windows下多iocp，也可以使用长连接.
		KPoolableSocketContainer::gcSocket(st, lifeTime, last_recv_time);
		kassert(vprocess!=NULL);
		vprocess->gcProcess(this);
	}
	void isBad(KUpstream *st,BadStage stage)
	{
		process.kill();
	}
	unsigned getSize()
	{
		return socket?1:0;
	}
	friend class KMPCmdProcess;
private:
	KTcpUpstream *socket;
	KMPCmdProcess *vprocess;
	sockaddr_i addr;
	time_t lastActive;
	KSingleListenPipeStream *next;
	KSingleListenPipeStream *prev;
};
struct MPVProcessPowerParam
{
        KHttpRequest *rq;
        KMPCmdProcess *process;
        KExtendProgram *rd;
};
#endif /* KCMDPROCESS_H_ */
