#ifndef KAPISERVICE_H
#define KAPISERVICE_H
#include "KApiEnv.h"
#include "httpext.h"
#include "KApiDso.h"
#include "forwin32.h"
class KApiService
{
public:
	KApiService(KApiDso *dso)
	{
#ifdef _WIN32
		hNotice = CreateEvent(NULL,FALSE,FALSE,NULL);
		asynCallBack = NULL;
		asynContext = NULL;
#endif
		headSended = false;
		this->dso = dso;
	}
	virtual ~KApiService()
	{
#ifdef _WIN32
		CloseHandle(hNotice);
#endif
	}
	int writeClient(const char *str, int len, bool async)
	{
		len = writeClient(str, len);
	#ifdef _WIN32
		if(async && asynCallBack) {
			asynCallBack(&ecb,asynContext,len,0);
		}
	#endif
		return len;
	}
	bool start()
	{
		assert(dso);
		if (!initECB(&ecb)) {
			return false;
		}
		dso->attachThread();
		//	ecb.lpszQueryString = (LPSTR) env.getEnv("QUERY_STRING");
		DWORD result = dso->HttpExtensionProc(&ecb);
		dso->detachThread();
		if (result == HSE_STATUS_PENDING) {
#ifdef _WIN32
			//300 seconds timeout
			WaitForSingleObject(hNotice,300000);
#endif
		}
		return true;
	}
	virtual int writeClient(const char *str, int len) = 0;
	virtual int readClient(char *buf, int len) = 0;
	virtual bool setStatusCode(const char *status, int len = 0) = 0;
	virtual bool addHeader(const char *attr, int len = 0) = 0;
	virtual bool execUrl(HSE_EXEC_URL_INFO *urlInfo) = 0;
	virtual bool initECB(EXTENSION_CONTROL_BLOCK *ecb) = 0;
	virtual Token_t getToken()
	{
		return NULL;
	}
	KApiEnv env;
	int leftRead;
	bool headSended;
#ifdef _WIN32
	HANDLE hNotice;
	PFN_HSE_IO_COMPLETION asynCallBack;
	LPDWORD asynContext;
#endif
	EXTENSION_CONTROL_BLOCK ecb;
	KApiDso *dso;
};
#endif
