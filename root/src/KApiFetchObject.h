/*
 * KApiFetchObject.h
 * isapi接口服务类，有两种服务提供方式，看该isapi模块的运行方式。
 * 一种是本地的，即isapi模块挂在kangle主进程上，以多线程的方式运行(服务提供者是KLocalApiService *sa)
 * 另一种是远程的，即isapi模块挂在kangle子进程上，以fastcgi(扩展了)的方式和主进程通信(服务提供者是KFastcgiStream *st)

 *  Created on: 2010-6-13
 *      Author: keengo
 */

#ifndef KAPIFETCHOBJECT_H_
#define KAPIFETCHOBJECT_H_
#include "KFetchObject.h"
#include "KHttpTransfer.h"
#include "KApiRedirect.h"
#include "KApiEnv.h"
#include "KHttpHeadPull.h"
#include "KHttpObjectParserHook.h"
#include "KFastcgiUtils.h"
#include "KSyncFetchObject.h"
#include "KApiService.h"

class KLocalApiService {
public:
	KHttpTransfer tr;
	KHttpObjectParserHook hook;
	KHttpHeadPull parse;
};
class KApiFetchObject: public KSyncFetchObject , public KApiService {
public:
	KApiFetchObject(KApiRedirect *rd);
	virtual ~KApiFetchObject();
	void process(KHttpRequest *rq);
	friend class KApiRedirect;
public:
	/*
	 *  sa 和 st两个必定要一个。
	 *　sa 存在是本进程内api调用。
	 *　st　存在是进程外api调用
	 */
	KLocalApiService sa;
	//KFastcgiStream *st;
	//KApiRedirect *rd;
	//KFileName *file;
public:
	int writeClient(const char *str, int len);
	int readClient(char *buf, int len);
	bool setStatusCode(const char *status, int len = 0);
	bool addHeader(const char *attr, int len = 0);
	bool execUrl(HSE_EXEC_URL_INFO *urlInfo);
	Token_t getToken();
	Token_t getVhToken(const char *vh_name);
	//const char *getVhEnv(const char *vh);

	Token_t token;
#ifndef _WIN32
	int id[2];
#endif
	KHttpRequest *rq;
private:
	bool responseDenied;
	bool initECB(EXTENSION_CONTROL_BLOCK *ecb);
};
#endif /* KAPIFETCHOBJECT_H_ */
