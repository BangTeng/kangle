/*
 * KProcessManage.h
 *
 *  Created on: 2010-8-17
 *      Author: keengo
 */
#ifndef KPROCESSMANAGE_H_
#define KPROCESSMANAGE_H_
#include <string.h>
#include <map>
#include <vector>
#include <list>
#include <time.h>
#include "global.h"
#include "KRedirect.h"
#include "KVirtualHost.h"
#include "KSocket.h"
#include "KMutex.h"
#include "KPipeStream.h"
//#include "api_child.h"
#include "KApiPipeStream.h"
#include "KCmdProcess.h"
#include "KVirtualHostProcess.h"
#include "KApiProcess.h"
class KApiRedirect;
class KApiProcessManage;

/*
 * 进程管理器。
 * 一个用户名对应一个虚拟进程.
 * 一个虚拟进程管理连接和实际进程(可能是一组进程，也可能是一个进程)
 * 具体说有两种模式，一种是SP(单进程，多线程),另一种是MP(多进程,单线程)
 *
 */
class KProcessManage {
public:
	KProcessManage() {

	}
	virtual ~KProcessManage();
	void connect(KHttpRequest *rq,KExtendProgram *rd);
	void clean();
	void refresh(time_t nowTime);
	void getProcessInfo(std::stringstream &s);
	
	void killAllProcess(KVirtualHost *vh=NULL);
	bool killProcess(const char *user,int pid);
	bool killProcess2(USER_T user,int pid);
	void setName(const char *name) {
		this->name = name;
	}
protected:
	virtual KVirtualHostProcess *newVirtualProcess() = 0;
private:
	KVirtualHostProcess *refsVirtualHostProcess(std::string app,KExtendProgram *rd)
	{
		//printf("refs virtualhost process app [%s]\n",app.c_str());
		KVirtualHostProcess *gc = NULL;
		std::map<USER_T, KVirtualHostProcess *>::iterator it;
		lock.Lock();
		it = pools.find(app);
		if (it != pools.end()) {
			gc = (*it).second;
			if (gc->status == VProcess_Close) {
				debug("process is closed,now recreate it.\n");
				gc->release();
				pools.erase(it);
				gc = NULL;
			} else {
				gc->addRef();
			}
		}
		if (gc == NULL) {
			debug("app [%s] gc is NULL\n",app.c_str());
			gc = newVirtualProcess();
			assert(gc);
			gc->setLifeTime(rd->lifeTime);
			//gc->setRefresh(true);
			gc->idleTime = rd->idleTime;
			gc->max_error_count = rd->max_error_count;
			gc->max_request = rd->maxRequest;
			//gc->maxConnect = rd->maxConnect;
			gc->addRef();
			pools.insert(std::pair<USER_T, KVirtualHostProcess *> (app, gc));
		}
		lock.Unlock();
		return gc;
	}
	/*
	 * 进程管理器名称
	 */
	std::string name;
	KMutex lock;
	std::map<USER_T, KVirtualHostProcess *> pools;
};
/*
 * api的进程管理器
 */
class KApiProcessManage: public KProcessManage {
public:
	KApiProcessManage() {

	}
	~KApiProcessManage() {

	}
protected:
	KVirtualHostProcess *newVirtualProcess() {
		return new KApiProcess;
	}
};

/*
 * 命令进程管理器
 */
class KCmdProcessManage: public KProcessManage {
public:	
	KCmdProcessManage()
	{
		worker = 0;
	}
	void setWorker(int worker)
	{
		this->worker = worker;
	}
protected:

	KVirtualHostProcess *newVirtualProcess() {
		if (worker==0) {
			return new KMPCmdProcess;
		}
		return new KCmdProcess;
	}

private:
	int worker;
};
extern KApiProcessManage spProcessManage;
#endif /* KPROCESSMANAGE_H_ */
