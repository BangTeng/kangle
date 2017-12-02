/*
 * KApiFetchObject.h
 * isapi�ӿڷ����࣬�����ַ����ṩ��ʽ������isapiģ������з�ʽ��
 * һ���Ǳ��صģ���isapiģ�����kangle�������ϣ��Զ��̵߳ķ�ʽ����(�����ṩ����KLocalApiService *sa)
 * ��һ����Զ�̵ģ���isapiģ�����kangle�ӽ����ϣ���fastcgi(��չ��)�ķ�ʽ��������ͨ��(�����ṩ����KFastcgiStream *st)

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
	 *  sa �� st�����ض�Ҫһ����
	 *��sa �����Ǳ�������api���á�
	 *��st�������ǽ�����api����
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
