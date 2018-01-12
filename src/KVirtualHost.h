/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 *
 * kangle web server              http://www.kangleweb.net/
 * ---------------------------------------------------------------------
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *  See COPYING file for detail.
 *
 *  Author: KangHongjiu <keengo99@gmail.com>
 */
#ifndef KVIRTUALHOST_H
#define KVIRTUALHOST_H
/**
 * virtual host
 */
#include <time.h>
#include <string>
#include <map>
#include "global.h"
#include "KLogElement.h"
//#include "KLocalFetchObject.h"
#include "KFileName.h"
#include "KCountable.h"
#include "KVirtualHost.h"
#include "KSubVirtualHost.h"
#include "KPathRedirect.h"
#include "KAccess.h"
#include "KUserManageInterface.h"
#include "utils.h"
#include "KBaseVirtualHost.h"
#include "KRequestQueue.h"
#include "extern.h"
#include "KFlowInfo.h"

class KTempleteVirtualHost;
/**
* 属性辅助类
*/
class KAttributeHelper
{
public:
	KAttributeHelper(std::map<std::string,std::string> *attribute)
	{
		this->attribute = attribute;
	}
	std::map<std::string,std::string> *getAttribute()
	{
		return attribute;
	}
	bool getValue(const char *name,std::string &value)
	{
		if(attribute==NULL){
			return false;
		}
		std::map<std::string,std::string>::iterator it;
		it = attribute->find(name);
		if(it==attribute->end()){
			return false;
		}
		value = (*it).second;
		return true;
	}
	void setValue(const char *name,const char *value)
	{
		attribute->insert(std::pair<std::string,std::string>(name,value));
	}
private:
	std::map<std::string,std::string> *attribute;
};
/**
* 连接数限制类
*/
class KConnectionLimit : public KCountable
{
public:
	KConnectionLimit()
	{
		refs = 1;
		cur_connect = 0;
	}
	bool addConnection(int max_connect)
	{
		refsLock.Lock();
		if(cur_connect >= max_connect){
			refsLock.Unlock();
			return false;
		}
		cur_connect ++;
		refsLock.Unlock();
		return true;
	}
	void releaseConnection()
	{
		refsLock.Lock();
		cur_connect --;
		refsLock.Unlock();
	}
	int getConnectionCount()
	{
		int count ;
		refsLock.Lock();
		count = cur_connect;
		refsLock.Unlock();
		return count;
	}
private:
	int cur_connect;
};
/**
* 虚拟主机类
*/
class KVirtualHost: public KBaseVirtualHost, public KCountable {
public:
	KVirtualHost();
	virtual ~KVirtualHost();
	bool setDocRoot(std::string &docRoot);
	KSubVirtualHost *getFirstSubVirtualHost() {
		if (hosts.size() == 0) {
			return NULL;
		}
		return *(hosts.begin());
	}
	virtual bool isTemplete()
	{
		return false;
	}
	friend class KHttpRequest;
	friend class KNsVirtualHost;
	friend class KVirtualHostManage;
	bool browse;
	//bool updatedFlag;
	/*
	 * 模板
	 */
	KTempleteVirtualHost *tvh;
#ifdef ENABLE_USER_ACCESS	
	int checkRequest(KHttpRequest *rq);
	int checkResponse(KHttpRequest *rq);
	int checkPostMap(KHttpRequest *rq);
	std::string user_access;
#endif
	std::string doc_root;
	std::string orig_doc_root;
#ifdef ENABLE_VH_LOG_FILE
	KLogElement *logger;
	std::string logFile;

	//废弃
	void setLogFile(std::string &path,
			std::map<std::string, std::string>&attribute);
	void setLogFile(KAttributeHelper *ah,KVirtualHost *tm=NULL);
#endif
	/*
	 * 计算是否需要杀掉对应的进程,返回true则要杀掉进程，否则不杀掉进程
	 */
	bool caculateNeedKillProcess(KVirtualHost *ov);
#ifdef ENABLE_VH_QUEUE
	void initQueue(KVirtualHost *ov)
	{
		if(queue){
			return;
		}
		if(max_worker>0){
			if(ov){
				queue = ov->queue;
			}
			if(queue){
				queue->addRef();
			}else{
				queue = new KRequestQueue;
			}
			queue->set(max_worker,max_queue);
		}
	}
	unsigned getWorkerCount();
	unsigned getQueueSize();
	KRequestQueue *queue;
	unsigned max_worker;
	unsigned max_queue;
#endif
#ifdef ENABLE_VH_RS_LIMIT
	KSpeedLimit *refsSpeedLimit()
	{
		KSpeedLimit *sl = NULL;
		lock.Lock();
		sl = this->sl;
		if (sl) {
			sl->addRef();
		}
		lock.Unlock();
		return sl;
	}
	void setSpeedLimit(const char *speed_limit_str,KVirtualHost *ov);
	void setSpeedLimit(int speed_limit,KVirtualHost *ov);
	
	int getConnectCount();
	bool addConnection() {
		if(cur_connect==NULL || max_connect==0){
			return true;
		}
		return cur_connect->addConnection(max_connect);
	}
	void releaseConnection() {
		if(cur_connect){
			cur_connect->releaseConnection();
		}
	}
	void initConnect(KVirtualHost *ov)
	{
		if(cur_connect){
			return;
		}
		if(max_connect>0){
			if(ov){
				cur_connect = ov->cur_connect;
			}
			if(cur_connect){
				cur_connect->addRef();
			}else{
				cur_connect = new KConnectionLimit;
			}
		}
	}
	//当前连接数信息
	KConnectionLimit *cur_connect;
	//连接数限制
	int max_connect;
	//带宽限制
	int speed_limit;
#endif
#ifdef ENABLE_VH_FLOW
	KFlowInfo *refsFlowInfo()
	{
		KFlowInfo *flow = NULL;
		lock.Lock();
		flow = this->flow;
		if (flow) {
			flow->addRef();
		}
		lock.Unlock();
		return flow;
	}
	void setFlow(bool fflow,KVirtualHost *ov)
	{
		lock.Lock();
		if (flow) {
			flow->release();
			flow = NULL;
		}
		this->fflow = fflow;
		if (fflow) {
			if (ov) {
				flow = ov->flow;
			}
			if (flow) {
				flow->addRef();
			} else {
				flow = new KFlowInfo;
			}
		}
		lock.Unlock();
	}
	int getSpeed(bool reset)
	{
		if (flow) {
			return flow->getSpeed(reset);
		}
		return 0;
	}
	//统计流量
	bool fflow;
#endif
	/*
	虚拟主机状态，0表示开通状态，其它表示暂停状态
	*/
	int status;
	KFetchObject *findPathRedirect(KHttpRequest *rq, KFileName *file,const char *path,
			bool fileExsit,bool &result);
	/*
	 check if a file will map to the rd
	 */
	bool isPathRedirect(KHttpRequest *rq, KFileName *file, bool fileExsit,
			KRedirect *rd);
	KFetchObject *findFileExtRedirect(KHttpRequest *rq, KFileName *file,
			bool fileExsit,bool &result);
	KFetchObject *findDefaultRedirect(KHttpRequest *rq,KFileName *file,bool fileExsit);
	//int instance_id;
	std::string name;
	bool ext;
	bool db;
#ifdef ENABLE_BASED_PORT_VH
	std::list<std::string> binds;
#endif
	bool empty()
	{
		if (!hosts.empty()) {
			return false;
		}
		return true;
	}
	std::list<KSubVirtualHost *> hosts;
#ifdef ENABLE_VH_RUN_AS
	std::string add_dir;
#ifndef _WIN32
	bool chroot;
#endif
	bool setRunAs(std::string user, std::string group);


	int id[2];
	/*
	 * run user
	 */
	std::string user;
	/*
	 * run group
	 */
	std::string group;
	std::string getUser()
	{
		return user;
	}
#ifdef _WIN32
	HANDLE logon(bool &result);
	Token_t getLogonedToken(bool &result)
	{
		result = logoned;
		return token;
	}
#endif
	Token_t createToken(bool &result);
	Token_t getProcessToken(bool &result);
	static void createToken(Token_t token);	
#else
	/*
	对于不支持虚拟主机运行用户时返回一个全局用户名
	*/
	std::string getUser() {
		return "-";
	}
#endif
	/*
	应用程序池数量
	*/
	int app;
	bool ip_hash;
	std::vector<std::string> apps;
	int app_share;
	void setApp(int app);
	std::string getApp(KHttpRequest *rq);
	static void closeToken(Token_t token);
	virtual void buildXML(std::stringstream &s);
	bool inherit;
	//是否支持串接请求
	bool concat;
	bool loadApiRedirect(KApiPipeStream *st,int workType);
	bool saveAccess();
	void setAccess(std::string access_file);
	std::string htaccess;
	KAccess access[2];
#ifdef ENABLE_HTTP2
	bool http2;
#endif
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
	std::string certfile;
	std::string keyfile;
	char *cipher;
	char *protocols;
	SSL_CTX *ssl_ctx;
	bool setSSLInfo(std::string certfile,std::string keyfile,std::string cipher,std::string protocols);
	std::string getCertfile();
	std::string getKeyfile();
#endif
private:
#ifdef ENABLE_VH_RS_LIMIT
	//当前带宽信息
	KSpeedLimit *sl;
#endif
#ifdef ENABLE_VH_FLOW
	//流量表
	KFlowInfo *flow;
#endif
	bool loadApiRedirect(KRedirect *rd,KApiPipeStream *st,int workType);
#ifdef ENABLE_VH_RUN_AS
#ifdef _WIN32
	HANDLE token;
	bool logoned;
	bool logonresult;
#endif
#endif
public:
#ifdef ENABLE_USER_ACCESS
	bool loadAccess(KVirtualHost *vh=NULL);
	time_t lastModified;
	time_t lastLoad;
#endif
};


#endif /*KVIRTUALHOST_H_*/
