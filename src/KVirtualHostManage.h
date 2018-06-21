/*
 * Kconf.gvm->h
 *
 *  Created on: 2010-4-19
 *      Author: keengo
 */
#ifndef KVIRTUALHOSTMANAGE_H_
#define KVIRTUALHOSTMANAGE_H_
#include <map>
#include <vector>
#include "global.h"
#define VH_CONFIG_FILE	"/etc/vh.xml"
#include "KXmlSupport.h"
#include "KVirtualHost.h"
#include "KNsVirtualHost.h"
#include "KMutex.h"
#include "KIpVirtualHost.h"
class KGTempleteVirtualHost;

class KVirtualHostManage {
public:
	KVirtualHostManage();
	virtual ~KVirtualHostManage();
	void copy(KVirtualHostManage *vm);
	void getMenuHtml(std::stringstream &s,KVirtualHost *vh,std::stringstream &url,int t);
	void getHtml(std::stringstream &s,std::string vh_name,int id, KUrlValue &attribute);
	void getListenHtml(std::stringstream &s);
	void build(std::stringstream &s);
	bool saveConfig(std::string &errMsg);
	bool vhAction(KVirtualHost *ov,KTempleteVirtualHost *tm,KUrlValue &attribute, std::string &errMsg);
	bool vhBaseAction(KUrlValue &attribute,std::string &errMsg);

	int getNextInstanceId();
	void getAutoName(std::string &name);
	void inheritVirtualHost(KVirtualHost *vh,bool clearFlag);
	void addAllVirtualHost();
	void flush_static_listens(std::vector<KListenHost *> &services);
	//bool startService(KListenHost *service);
	int getCount();
public:
	/*
	 * 更新虚拟主机
	 */
	bool updateVirtualHost(KVirtualHost *vh);
	bool updateVirtualHost(KVirtualHost *vh,KVirtualHost *ov);
	/*
	 * 增加虚拟主机
	 */
	bool addVirtualHost(KVirtualHost *vh);
	/*
	 * 删除虚拟主机
	 */
	bool removeVirtualHost(KVirtualHost *vh);
	/*
	 * 查找虚拟主机并绑定在rq上。
	 */
	void add_static(KServer *server);
	void remove_static(KServer *server);
	query_vh_result queryVirtualHost(KServer *ls,KSubVirtualHost **rq_svh,const char *site,int site_len);
	int find_domain(const char *domain, WhmContext *ctx);
	void getAllVh(std::list<std::string> &vhs,bool status,bool onlydb);
	bool getAllTempleteVh(const char *templeteGroup,std::list<std::string> &vhs);
	void getAllGroupTemplete(std::list<std::string> &vhs);
	KVirtualHost *refsVirtualHostByName(std::string name);
	KServer *refsServer(u_short port);
	KTempleteVirtualHost *refsTempleteVirtualHost(std::string name);
	bool updateTempleteVirtualHost(KTempleteVirtualHost *tvh);
	bool removeTempleteVirtualHost(std::string name);	
#ifdef ENABLE_VH_FLOW
	void dumpLoad(KVirtualHostEvent *ctx,bool revers,const char *prefix,int prefix_len);
	//导出所有虚拟主机流量
	void dumpFlow();
	void dumpFlow(KVirtualHostEvent *ctx,bool revers,const char *prefix,int prefix_len,int extend);
#endif
	/*
	把所有的virtualHost的updatedFlag=false,准备更新vh.xml文件。
	*/
	//void updateAllVirtualHost();
	/*
	已经更新完了vh.xml文件，把所有updatedFlag=false的删除,并杀掉相应的进程。
	*/
	//void checkAllVirtualHost();
	KBaseVirtualHost globalVh;
public:
	friend class KHttpServerParser;
	friend class KHttpManage;
private:
	void getVhIndex(std::stringstream &s,KVirtualHost *vh,int id,int t,u_short default_http_port);
	void inheriteAll();
	bool internalAddVirtualHost(KVirtualHost *vh,KVirtualHost *ov);
	bool internalRemoveVirtualHost(KVirtualHost *vh,bool removeIndex=true);
	//KNsVirtualHost *getNsVirtualHost(std::string name);
	void getAllVhHtml(std::stringstream &s,int tvh);
	void getVhDetail(std::stringstream &s, KVirtualHost *vh,bool edit,int t);
	void flushListen(KVirtualHost *vh);
	/*
	 * 所有虚拟主机列表
	 */
	std::map<std::string, KVirtualHost *> avh;
	/*
	 * 所有模板虚拟主机
	 */
	std::map<std::string, KGTempleteVirtualHost *> gtvhs;
	/*
	* 绑定所有的vh到侦听上
	*/
	void internalBindAllVirtualHost();
	/*
	* 绑定到侦听上
	*/
	void internalBindVirtualHost(KVirtualHost *vh);
private:
	KMutex lock;
	//KMutex envLock;
	KMutex instancelock;
	int curInstanceId;
};
#endif /* KVIRTUALHOSTMANAGE_H_ */
