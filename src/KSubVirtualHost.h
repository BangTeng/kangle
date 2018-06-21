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
	* ���url�������ļ���ת��
	* exsit��ʶ�ļ��Ƿ����
	* htresponse htaccessת�����access����
	* handled �Ƿ��Ѿ�������rq,��htaccess�Ѿ��������ݸ�rq(�ض���,�ܾ��ȵ�)
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
