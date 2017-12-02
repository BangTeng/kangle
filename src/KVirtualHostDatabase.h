#ifndef KVIRTUALHOSTDATABASE_H
#define KVIRTUALHOSTDATABASE_H
#include <map>
#include <list>
#include <string>
#include "global.h"
#include "utils.h"
#include "vh_module.h"
#include "KVirtualHost.h"
#include "KDsoModule.h"
#define VH_INFO_HOST       0
#define VH_INFO_ERROR_PAGE 1
#define VH_INFO_INDEX      2
#define VH_INFO_MAP        3
#define VH_INFO_ALIAS      4
#define VH_INFO_MIME       5
#define VH_INFO_BIND       7
#define VH_INFO_HOST2      8
#define VH_INFO_ENV        100

class KVirtualHostDatabase
{
public:
	KVirtualHostDatabase();
	~KVirtualHostDatabase();
	bool flushVirtualHost(const char *vhName,bool initEvent,KVirtualHostEvent *ctx);
	bool loadVirtualHost(KVirtualHostManage *vm,std::string &errMsg);
#ifdef ENABLE_VH_FLOW
	//流量操作
	//bool saveFlow(KVirtualHost *vh,void *cn);
#endif
	/*
	写入操作
	*/
	//bool addInfo(std::map<std::string,std::string> &attribute,std::string &errMsg,bool skipFlush=false);
	//bool delInfo(std::map<std::string,std::string> &attribute,std::string &errMsg,bool skipFlush=false);
	//bool addVirtualHost(std::map<std::string,std::string> &attr,KVirtualHostEvent *ctx,std::string &errMsg);
	//bool delVirtualHost(std::map<std::string,std::string> &attribute);

	//bool updateVirtualHost(KVirtualHostEvent *ctx,std::map<std::string,std::string> &attribute,std::string &errMsg);
	bool parseAttribute(std::map<std::string,std::string> &attribute);
	//检查数据库连接是否正常
	bool check();
	bool isSuccss()
	{
		return lastStatus;
	}
	bool isLoad();
	void clear();
	bool ext;
	void *createConnection();
	void freeConnection(void *cn);
private:
	//bool delInfo(KVirtualHostConnection *cn,const char *vhName,const char *name,int type,const char *value);

	bool loadInfo(KVirtualHost *vh,void *cn);
	//int getColIndex(const char *name);
	KVirtualHost *newVirtualHost(void *cn,std::map<std::string,std::string> &attribute,KVirtualHostManage *vm,KVirtualHost *ov);
	//bool buildVhAttribute(const char *name,KVirtualHostData *rs,std::map<std::string,std::string> &attribute);
	//std::map<char *,int,lessp_icase> colmap;
	KMutex lock;
	//KVirtualHostDataInterface *vhdi;
	vh_module vhm;
	bool lastStatus;
	KDsoModule vhm_handle;
};
extern KVirtualHostDatabase vhd;
#endif
