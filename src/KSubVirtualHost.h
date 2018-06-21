/*
 * KSubVirtualHost.h
 *
 *  Created on: 2010-9-2
 *      Author: keengo
 */

#ifndef KSUBVIRTUALHOST_H_
#define KSUBVIRTUALHOST_H_
#include <string>
#include "KFileName.h"
#include "KHttpRequest.h"

#ifdef ENABLE_SUBDIR_PROXY
enum subdir_type
{
	subdir_local,
	subdir_proxy,
	subdir_redirect
};
#endif

class KVirtualHost;


class KSubVirtualHost {
public:
	KSubVirtualHost(KVirtualHost *vh);
	virtual ~KSubVirtualHost();
	void setDocRoot(const char *doc_root,const char *dir);
	bool equale(KSubVirtualHost *svh)
	{
		if(strcmp(host,svh->host)!=0){
			return false;
		}
		if(strcmp(dir,svh->dir)!=0){
			return false;
		}
		return true;
	}
	void release();
	bool setHost(const char *host);
	/**
	* 完成url到物理文件的转换
	* exsit标识文件是否存在
	* htresponse htaccess转换后的access对象
	* handled 是否已经处理了rq,如htaccess已经发送数据给rq(重定向,拒绝等等)
	*/
	bool bindFile(KHttpRequest *rq,KHttpObject *obj,bool &exsit,KAccess **htresponse,bool &handled);
	bool bindFile(KHttpRequest *rq,bool &exsit,bool searchDefaultFile,bool searchAlias);
	char *mapFile(const char *path);
	char *host;
	char *dir;
	char *doc_root;
	KVirtualHost *vh;
	domain_t bind_host;
	bool wide;
	bool allSuccess;
	bool fromTemplete;
	
#ifdef ENABLE_SUBDIR_PROXY
	subdir_type type;
	KUrl *dst;
	char *ip;
	
	int lifeTime;
	char *http_proxy;
	char *https_proxy;
#endif
private:
	
	bool makeHtaccess(const char *prefix,KFileName *file,KAccess *request,KAccess *response);
};

#endif /* KSUBVIRTUALHOST_H_ */
