#ifndef KDYNAMICLISTEN_H
#define KDYNAMICLISTEN_H
#include <map>
#include "KServer.h"
#include "do_config.h"
/*
* 侦听端口管理。
* 即由virtualhost的<bind>!ip:port</bind>
*/
class KListenKey
{
public:

	bool operator < (const KListenKey &a) const
	{
		int ret = strcmp(ip.c_str(),a.ip.c_str());
		if (ret < 0) {
			return true;
		}
		if (ret > 0) {
			return false;
		}
		if (port < a.port) {
			return true;
		}
		if (port > a.port) {
			return false;
		}
		return ipv4 < a.ipv4;
	}
	std::string ip;
	int port;
	bool ipv4;
	bool ssl;
	
};
/**
* 由virtualHostManager加锁调用
*/
class KDynamicListen
{
public:
	KDynamicListen()
	{
		failedTries = 0;
	}
	KServer *refsServer(u_short port);
	void add_dynamic(const char *listen,KVirtualHost *vh);
	void remove(const char *listen,KVirtualHost *vh);
	bool add_static(KListenHost *lh);
	void addStaticVirtualHost(KVirtualHost *vh);
	void removeStaticVirtualHost(KVirtualHost *vh);
	void flush(const char *listen);
	void flush();
	void delayStart();
	void getListenHtml(std::stringstream &s);
	void clear();
	void close();
	std::map<KListenKey,KServer *> listens;
private:	
	void parseListen(const char *listen,std::list<KListenKey> &lk);
	void parseListen(KListenHost *lh,std::list<KListenKey> &lk);
	bool initListen(const KListenKey &lk,KServer *server);
	void getListenKey(KListenHost *lh,bool ipv4,std::list<KListenKey> &lk);
	void getListenKey(KListenHost *lh,int port,bool ipv4,std::list<KListenKey> &lk);
	int failedTries; 
};
extern KDynamicListen dlisten;
#endif
