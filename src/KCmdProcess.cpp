/*
 * KCmdProcess.cpp
 *
 *  Created on: 2010-10-24
 *      Author: keengo
 */
#include <vector>
#include "KCmdProcess.h"
#include "lang.h"
#include "KCmdPoolableRedirect.h"
#include "KAsyncFetchObject.h"

#ifdef ENABLE_VH_RUN_AS
//启动进程工作线程
FUNC_TYPE FUNC_CALL MPVProcessPowerWorker(void *param)
{
        MPVProcessPowerParam *vpp = (MPVProcessPowerParam *)param;
        bool success;
        KUpstreamSelectable *socket = vpp->process->poweron(vpp->rq->svh->vh,vpp->rd,success);
		if(socket){
			vpp->rq->c->selector->bindSelectable(socket);
		}
        static_cast<KAsyncFetchObject *>(vpp->rq->fetchObj)->connectCallBack(vpp->rq,socket,false);
        vpp->process->release();
        delete vpp;
		KTHREAD_RETURN;
}
KCmdProcess::KCmdProcess() {
	st = NULL;
}
KCmdProcess::~KCmdProcess() {
	if (st) {
		delete st;
	}
}
KUpstreamSelectable *KCmdProcess::poweron(KVirtualHost *vh,KExtendProgram *erd,bool &success)
{
	int port = 0;
	KCmdPoolableRedirect *rd = static_cast<KCmdPoolableRedirect *> (erd);
	KUpstreamSelectable *socket = NULL;
	KListenPipeStream *st2 = new KListenPipeStream;
	unix_path.clear();
	socket = rd->createPipeStream(vh,st2,unix_path,false);
	if (socket == NULL) {
		delete st2;
		success = false;
		return NULL;
	}
	stLock.Lock();
	if (st) {
		delete st;
	}	
	st = st2;
	//这里把端口号保存，下次连接时就不用对stLock加锁了。
	success = true;
	if (unix_path.size()==0) {
		port = st->getPort();	
		KSocket::getaddr("127.0.0.1",port,&addr);
	}
	stLock.Unlock();
	return socket;
}
KMPCmdProcess::KMPCmdProcess()
{
	freeProcessList = new KSingleListenPipeStream;
	busyProcessList = new KSingleListenPipeStream;
	klist_init(freeProcessList);
	klist_init(busyProcessList);
}
KMPCmdProcess::~KMPCmdProcess()
{
	KSingleListenPipeStream *st;
	for (;;) {
		st = klist_head(freeProcessList);
		if (st == freeProcessList) {
			break;
		}
		klist_remove(st);
		delete st;
	}
	for (;;) {
		st = klist_head(busyProcessList);
		if (st == busyProcessList) {
			break;
		}
		klist_remove(st);
		delete st;
	}
	delete freeProcessList;
	delete busyProcessList;
}
void KMPCmdProcess::handleRequest(KHttpRequest *rq,KExtendProgram *rd)
{
	KSingleListenPipeStream *sp = NULL;
    stLock.Lock();
	if (!klist_empty(freeProcessList)) {
		sp = klist_head(freeProcessList);
		klist_remove(sp);
		klist_append(busyProcessList, sp);
	}
    stLock.Unlock();
	if (sp) {
		addRef();
		bool isHalf;
		KUpstreamSelectable *socket = sp->getConnection(rq, isHalf);
		if (socket == NULL) {
			sp->killChild();
			gcProcess(sp);
		}
		static_cast<KAsyncFetchObject *>(rq->fetchObj)->connectCallBack(rq, socket, isHalf);
		return;
	}
	if (sp == NULL) {
		//rq->c->removeRequest(rq, true);
		MPVProcessPowerParam *param = new MPVProcessPowerParam;
		param->rq = rq;
		param->rd = rd;
		param->process = this;
		addRef();
		if (!m_thread.start(param, MPVProcessPowerWorker)) {
			static_cast<KAsyncFetchObject *>(rq->fetchObj)->connectCallBack(rq, NULL, true);
			delete param;
			release();
		}
	}
}
KUpstreamSelectable *KMPCmdProcess::poweron(KVirtualHost *vh,KExtendProgram *erd,bool &success)
{
	KCmdPoolableRedirect *rd = static_cast<KCmdPoolableRedirect *> (erd);
	KUpstreamSelectable *socket = NULL;
	KSingleListenPipeStream *st = new KSingleListenPipeStream;
#ifdef KSOCKET_UNIX
	if (conf.unix_socket) {
		std::stringstream s;
		s << "/tmp/kangle_" << getpid() << (void *)st << ".sock";
		//s.str().swap(st->unix_path);
		if (!st->listen(s.str().c_str())) {
			delete st;
			return NULL;
		}
	} else {
#endif
		if (!st->listen(0,"127.0.0.1")) {
			delete st;
			return NULL;
		}
#ifdef KSOCKET_UNIX
	}
#endif
	//we need cmdLock and the stLock
	cmdLock.Lock();
	st->setLifeTime(this->lifeTime);
	socket = rd->createPipeStream(vh,st,st->unix_path,!klist_empty(busyProcessList));
	if (socket == NULL) {
		cmdLock.Unlock();
		delete st;
		return NULL;
	}
#if 0

	
#endif
	stLock.Lock();
	klist_append(busyProcessList, st);
    stLock.Unlock();
	cmdLock.Unlock();
	st->bind(socket);
	st->vprocess = this;
	addRef();
	//这里把端口号保存，下次连接时就不用对stLock加锁了。
	if (st->unix_path.size()==0) {
		int port = st->getPort();
		KSocket::getaddr("127.0.0.1",port,&st->addr);
	}
	return socket;
}
void KMPCmdProcess::gcProcess(KSingleListenPipeStream *st)
{
	bool isKilled = st->process.isKilled();
	stLock.Lock();
	klist_remove(st);
	if (!isKilled) {
		KSingleListenPipeStream *head = freeProcessList->next;
		klist_insert(head,st);
	}
	stLock.Unlock();	
	if (isKilled) {
		delete st;
	}
	release();
}

void KMPCmdProcess::getProcessInfo(const USER_T &user, const std::string &name,std::stringstream &s,int &count)
{
	KSingleListenPipeStream *st;
	stLock.Lock();
	klist_foreach(st, busyProcessList) {
		count++;
		::getProcessInfo(user, name, &st->process, st, s);
	}
	klist_foreach(st, freeProcessList) {
		count++;
		::getProcessInfo(user, name, &st->process, st, s);
	}
	stLock.Unlock();
}
bool KMPCmdProcess::killProcess(int pid)
{
	KSingleListenPipeStream *st;
	bool successKilled = false;
	stLock.Lock();
	klist_foreach(st, busyProcessList) {
		if (pid == 0) {
			st->killChild();
			st->unlink_unix();
			continue;
		}
		if (st->process.getProcessId() == pid) {
			st->killChild();
			st->unlink_unix();
			successKilled = true;
			break;
		}
	}
	if (!successKilled) {
		klist_foreach(st, freeProcessList) {
			if (pid == 0) {
				st->killChild();
				st->unlink_unix();
				continue;
			}
			if (pid == st->process.getProcessId()) {
				st->killChild();
				st->unlink_unix();
				klist_remove(st);
				delete st;
				break;
			}
		}		
	}
	stLock.Unlock();
	if (pid==0) {
		return true;
	}
	return false;
}
bool KMPCmdProcess::canDestroy(time_t nowTime)
{
	bool result = false;
	KSingleListenPipeStream *st;
	stLock.Lock();
	for (;;) {
		st = klist_end(freeProcessList);
		if (st == freeProcessList) {
			break;
		}
		if (nowTime < idleTime + st->lastActive) {
			break;
		}
		klist_remove(st);
		delete st;
	}
	if(klist_empty(freeProcessList) && klist_empty(busyProcessList)) {
		result = true;
	}
	stLock.Unlock();
	return result;
}
#endif
