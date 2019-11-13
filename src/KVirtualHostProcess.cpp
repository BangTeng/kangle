/*
 * KVirtualProcess.cpp
 *
 *  Created on: 2010-10-24
 *      Author: keengo
 */
#include <vector>
#include <list>
#include "KVirtualHostProcess.h"
#include "KAsyncFetchObject.h"
#include "lang.h"
using namespace std;
//启动进程失败的情况的处理
void handleVProcessPower(VProcessPowerParam *vpp,std::list<KHttpRequest *> &queue,bool success,KTcpUpstream *socket,bool half_connect)
{
	if (vpp->rq) {
		//如果有第一个请求，把首次连接给他
		if (socket) {
			socket->BindSelector(vpp->rq->sink->GetSelector());
		}
		KUpstream *us = socket;
		if (success){
			if (us==NULL) {
				half_connect = false;
				us = vpp->process->connect(vpp->rq,vpp->rd,half_connect);
			}
		} else {
			assert(socket==NULL);
			//socket cann't be NULL.but is socket is not NULL.we also can handle it.
			if (us) {
				us->Destroy();
				us = NULL;
			}
		}
		assert(vpp->rq && vpp->process && vpp->rd);
		static_cast<KAsyncFetchObject *>(vpp->rq->fetchObj)->connectCallBack(vpp->rq, us,half_connect);
	} else {
		//没有第一个请求，把首次连接删除
		if (socket) {
			socket->Destroy();
			socket = NULL;
		}
	}
	list<KHttpRequest *>::iterator it;
	for (it = queue.begin(); it!=queue.end(); it++) {
		KUpstream *us = NULL;
		if (success) {
			us = vpp->process->connect((*it),vpp->rd,half_connect);
		}
		static_cast<KAsyncFetchObject *>((*it)->fetchObj)->connectCallBack((*it),us,half_connect);
	}
}
//启动进程工作线程
KTHREAD_FUNCTION VProcessPowerWorker(void *param)
{
	VProcessPowerParam *vpp = (VProcessPowerParam *)param;
	assert(vpp);
	KVirtualHostProcess *process = vpp->process;
	assert(process);
	bool success;
	KVirtualHost *vh = vpp->vh;
	if (vh==NULL && vpp->rq) {
		vh = vpp->rq->svh->vh;
	}
	KTcpUpstream *socket = process->poweron(vh,vpp->rd,success);

	if (socket) {
		//废弃第一条连接，目前不稳定.
		socket->Destroy();
		socket = NULL;
	}
	std::list<KHttpRequest *> queue;
	process->lock.Lock();
	queue.swap(process->queue);
	if (success) {
		process->status = VProcess_Poweron;
	} else {
		process->status = VProcess_Poweroff;
	}
	process->lock.Unlock();
	handleVProcessPower(vpp,queue,success,socket,false);
	delete vpp;
	process->release();
	KTHREAD_RETURN;
}
void getProcessInfo(const USER_T &user,const std::string &name,KProcess *process,KPoolableSocketContainer *ps,std::stringstream &s)
{
	//time_t totalTime = time(NULL) - process->startTime;
	s << "<tr>";
	s << "<td>[<a href='/process_kill?name=" << name << "&user=" << user;
	s << "&pid=" << process->getProcessId() << "'>" << klang["kill"] << "</a>]</td>";
	s << "<td>" << user << "</td>";
	s << "<td >" << process->getProcessId() << "</td>";

	s << "<td>" << (ps->getRef() - 1) << "</td>";
	s << "<td>" << ps->getSize() << "</td>";
	s << "<td>" << (kgl_current_sec - process->getPowerOnTime()) << "</td>";
	s << "</tr>\n";

}
kev_result KVirtualHostProcess::handleRequest(KHttpRequest *rq,KExtendProgram *rd)
{
	bool isHalf;
	if (status == VProcess_Poweron) {
		KUpstream *socket = connect(rq,rd,isHalf);
		return static_cast<KAsyncFetchObject *>(rq->fetchObj)->connectCallBack(rq,socket,isHalf);
	}
	lock.Lock();
	switch (status) {
	case VProcess_Poweroff:
	case VProcess_Close:
		{
			//rq->c->removeRequest(rq,true);
			//gc->queue.push_back(rq);
			VProcessPowerParam *param = new VProcessPowerParam;
			addRef();
			param->vh = rq->svh->vh;
			param->rq = rq;
			param->process = this;
			param->rd = rd;
			status = VProcess_Inprogress;			
			if(!kthread_pool_start(VProcessPowerWorker, param)){
				std::list<KHttpRequest *> tq;
				tq.swap(this->queue);
				lock.Unlock();
				handleVProcessPower(param,tq,false,NULL,false);
				delete param;
				this->release();
				return kev_ok;
			}
			break;
		}
	case VProcess_Inprogress:
		{
			//rq->c->removeRequest(rq,true);
			queue.push_back(rq);
			break;
		}
	case VProcess_Poweron:
		{
			lock.Unlock();
			KUpstream *socket = connect(rq,rd,isHalf);
			return static_cast<KAsyncFetchObject *>(rq->fetchObj)->connectCallBack(rq,socket,isHalf);
		}
	}
	lock.Unlock();
	return kev_ok;
}
