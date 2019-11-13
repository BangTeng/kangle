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
#include "KVirtualHostContainer.h"
#include "krbtree.h"

enum subdir_type
{
	subdir_local,
	subdir_http,
	subdir_server,
	subdir_portmap
};

class SubdirHttp
{
public:
	SubdirHttp()
	{
		memset(this, 0, sizeof(SubdirHttp));
	}
	~SubdirHttp() {
	}
	KUrl dst;
	char *ip;
	
	int lifeTime;
};
class SubdirServer
{
public:
	SubdirServer()
	{
		memset(this, 0, sizeof(SubdirServer));
	}
	~SubdirServer()
	{
		if (http_proxy) {
			xfree(http_proxy);
		}		
	}
	char *http_proxy;
	char *https_proxy;
};
iterator_ret subdir_port_map_destroy(void *data, void *argv);
struct subdir_port_map_item
{
	int port;
	char *proxy;
};
struct SubdirPortMap
{
public:
	SubdirPortMap()
	{
		tree = rbtree_create();
	}
	~SubdirPortMap()
	{
		destroy();
		rbtree_destroy(tree);
	}
	void add(int port, const char *proxy);
	const char *find(int port);
private:
	void destroy()
	{
		rbtree_iterator(tree, subdir_port_map_destroy, NULL);
	}
	krb_tree *tree;
};
class KVirtualHost;


class KSubVirtualHost {
public:
	KSubVirtualHost(KVirtualHost *vh);
	virtual ~KSubVirtualHost();
	void setDocRoot(const char *doc_root,const char *dir);
	bool equale(KSubVirtualHost *svh)
	{
		if (strcmp(host,svh->host)!=0) {
			return false;
		}		
		if (strcmp(dir,svh->dir)!=0) {
			return false;
		}
		return true;
	}
	void release();
	bool MatchHost(const char *host);
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
	void free_subtype_data();
	char *host;
	KVirtualHost *vh;
	domain_t bind_host;
	bool wide;
	bool allSuccess;
	bool fromTemplete;
	subdir_type type;
	
	char *dir;
	union {
		char *doc_root;
		SubdirHttp *http;
		SubdirServer *server;
		SubdirPortMap *portmap;
	};
private:
	
	bool makeHtaccess(const char *prefix,KFileName *file,KAccess *request,KAccess *response);
};
#endif /* KSUBVIRTUALHOST_H_ */
