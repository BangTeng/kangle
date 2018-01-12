/*
 * KCmdProcess.h
 *
 *  Created on: 2010-10-24
 *      Author: keengo
 */

#ifndef KCMDPROCESS_H_
#define KCMDPROCESS_H_

#include "KVirtualHostProcess.h"
#include "KList.h"
#include "KListenPipeStream.h"
#include "KSelectorManager.h"
/*
多线程命令进程
*/
class KCmdProcess: public KVirtualHostProcess {
public:
	KCmdProcess();
	~KCmdProcess();
	KUpstreamSelectable *poweron(KVirtualHost *vh,KExtendProgram *rd,bool &success);
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
	void handleRequest(KHttpRequest *rq,KExtendProgram *rd);
	void getProcessInfo(const USER_T &user, const std::string &name,std::stringstream &s,int &count);
	bool killProcess(int pid);
	KUpstreamSelectable *poweron(KVirtualHost *vh,KExtendProgram *erd,bool &success);
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
			socket->destroy();
		}
		unlink_unix();
	}
	KUpstreamSelectable *getConnection(KHttpRequest *rq,bool &isHalf)
	{
		lastActive = kgl_current_sec;
		KUpstreamSelectable *st = socket;
		if(socket == NULL) {
			st = KPoolableSocketContainer::getPoolSocket(rq);
			if (st) {
				isHalf = false;
				return st;
			}
			st = new KUpstreamSelectable(new KClientSocket);
#ifdef KSOCKET_UNIX
			if(unix_path.size()>0){
					st->socket->halfconnect(unix_path.c_str());
			} else 
#endif
				st->socket->halfconnect(addr);
			isHalf = true;
		} else {	
			isHalf = false;
			socket = NULL;
		}
		if (st) {
			bind(st);
		}
		return st;
	}
	void gcSocket(KUpstreamSelectable *st,int lifeTime)
	{
#ifdef _WIN32
		if (selectorManager.getSelectorCount() == 1) {
			//windows下只有一个selector时才使用长连接
			KPoolableSocketContainer::gcSocket(st, lifeTime);
		} else {
			st->destroy();
		}
#else
		KPoolableSocketContainer::gcSocket(st, lifeTime);
#endif
		kassert(vprocess!=NULL);
		vprocess->gcProcess(this);
	}
	void isBad(KUpstreamSelectable *st,BadStage stage)
	{
		process.kill();
	}
	unsigned getSize()
	{
		return socket?1:0;
	}
	friend class KMPCmdProcess;
private:
	KUpstreamSelectable *socket;
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
