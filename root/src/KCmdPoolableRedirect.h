/*
 * KCmdPoolableRedirect.h
 *
 *  Created on: 2010-8-26
 *      Author: keengo
 */
#ifndef KCMDPOOLABLEREDIRECT_H_
#define KCMDPOOLABLEREDIRECT_H_
#include <map>
#include <vector>
#include <string.h>
#include "global.h"
#ifdef ENABLE_VH_RUN_AS
#include "KVirtualHost.h"
#include "KAcserver.h"
#include "KProcessManage.h"
#include "KExtendProgram.h"
#include "KSocket.h"
#include "KListenPipeStream.h"


/*
 * 命令扩展。
 * 支持进程模型为SP和MP
 * 支持多种协议，HTTP,AJP,FASTCGI等等。
 *
 */
class KCmdPoolableRedirect: public KPoolableRedirect, public KExtendProgram {
public:
	KCmdPoolableRedirect();
	virtual ~KCmdPoolableRedirect();
	unsigned getPoolSize() {
		return 0;
	}
	const char *getName() {
		return name.c_str();
	}
	void connect(KHttpRequest *rq);
	void parseConfig(std::map<std::string, std::string> &attribute);

	KProcessManage *getProcessManage() {
		return static_cast<KProcessManage *>(&pm);
	}
	KUpstreamSelectable *createPipeStream(KVirtualHost *vh, KListenPipeStream *st,std::string &unix_path,bool isSameRunning);
	bool parseEnv(std::map<std::string, std::string> &attribute);
	void buildXML(std::stringstream &s);
	const char *getType() {
		return "cmd";
	}
	void lockCommand()
	{
		lock.Lock();
	}
	void unlockCommand()
	{
		lock.Unlock();
	}
	bool isChanged(KPoolableRedirect *rd);
	bool chuser;
	bool lockGlobal;
	int worker;
	int port;
	int sig;
	friend class KSPCmdGroupConnection;	
	std::string cmd;
private:
	bool setWorkType(const char *typeStr,bool changed);
	KMutex lock;
	KCmdProcessManage pm;
};
#endif
#endif /* KCMDPOOLABLEREDIRECT_H_ */
