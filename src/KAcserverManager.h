#ifndef KACSERVERMANAGER_H_
/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */
#define KACSERVERMANAGER_H_
#include <vector>
#include "KApiRedirect.h"
#include "KSingleAcserver.h"
#include "KMultiAcserver.h"
#include "KCmdPoolableRedirect.h"
#include "KCgiRedirect.h"
#include "KRWLock.h"
#include "KExtendProgram.h"
#include "malloc_debug.h"
#include <string>
class KAcserverManager: public KXmlSupport {
public:
	KAcserverManager();
	virtual ~KAcserverManager();
	std::string acserverList(std::string name = "");
	std::string apiList(std::string name = "");
	std::string cgiList(std::string name = "");
#ifdef ENABLE_VH_RUN_AS	
	std::string cmdList(std::string name = "");
	
	void refreshCmd(time_t nowTime);
	void getProcessInfo(std::stringstream &s);
	void killCmdProcess(USER_T user);
	void killAllProcess(KVirtualHost *vh=NULL);
	/* ȫ��׼�����ˣ����Լ������е�api�ˡ�*/
	void loadAllApi();
#endif
	void copy(KAcserverManager &a);
	void unloadAllApi();
#ifdef ENABLE_MULTI_SERVER
	std::string macserverList(std::string name="");
	bool delMAcserver(std::string name, std::string &err_msg);	
	std::string macserver_node_form(std::string name, std::string action,unsigned nodeIndex);
	bool macserver_node(std::map<std::string, std::string> &attribute,std::string &errMsg);
	KMultiAcserver * refsMultiAcserver(std::string name)
	{
		lock.RLock();
		KMultiAcserver *as = getMultiAcserver(name);
		if(as){
			as->addRef();
		}
		lock.RUnlock();
		return as;
	}
	bool addMultiAcserver(KMultiAcserver *as)
	{
		lock.WLock();
		std::map<std::string,KMultiAcserver *>::iterator it = mservers.find(as->name);
		if (it!=mservers.end()) {
			lock.WUnlock();
			return false;
		}
		mservers.insert(std::pair<std::string,KMultiAcserver *>(as->name,as));
		lock.WUnlock();
		return true;
	}
#endif
	bool cgiForm(std::map<std::string, std::string> &attribute,
			std::string &errMsg);

	std::vector<std::string> getAcserverNames(bool onlyHttp);
	std::vector<std::string> getAllTarget();
	bool newSingleAcserver(bool overFlag,std::map<std::string, std::string> &attr, std::string &err_msg);
#ifdef ENABLE_VH_RUN_AS
	bool cmdForm(std::map<std::string, std::string> &attribute,
			std::string &errMsg);
	KCmdPoolableRedirect *newCmdRedirect(std::map<std::string, std::string> &attribute,
			std::string &errMsg);
	KCmdPoolableRedirect *refsCmdRedirect(std::string name);
	bool cmdEnable(std::string name, bool enable);
	bool delCmd(std::string name, std::string &err_msg);
	bool addCmd(KCmdPoolableRedirect *as)
	{
		lock.WLock();
		std::map<std::string,KCmdPoolableRedirect *>::iterator it = cmds.find(as->name);
		if (it!=cmds.end()) {
			lock.WUnlock();
			return false;
		}
		cmds.insert(std::pair<std::string,KCmdPoolableRedirect *>(as->name,as));
		lock.WUnlock();
		return true;
	}
#endif

	bool delAcserver(std::string name, std::string &err_msg);
	
	bool apiEnable(std::string name, bool enable);
	bool cgiEnable(std::string name, bool enable);

	bool delApi(std::string name, std::string &err_msg);
	bool delCgi(std::string name, std::string &err_msg);

	bool apiForm(std::map<std::string, std::string> &attribute,
			std::string &errMsg);
	KSingleAcserver *refsSingleAcserver(std::string name);
	KPoolableRedirect *refsAcserver(std::string name);
	KCgiRedirect *refsCgiRedirect(std::string name);
	KRedirect *refsRedirect(std::string target);
	KApiRedirect *refsApiRedirect(std::string name);
	void clearImportConfig();
	friend class KAccess;
	friend class KHttpManage;
public:
	bool startElement(std::string &context, std::string &qName, std::map<
			std::string, std::string> &attribute);
	bool startElement(KXmlContext *context, std::map<std::string,
			std::string> &attribute) ;
	bool endElement(std::string &context, std::string &qName);
	void buildXML(std::stringstream &s, int flag);
private:

	bool newCgiRedirect(std::string name, std::string cmd, std::string arg,
			std::string env, std::string env_split, std::string &err_msg);
	bool newApiRedirect(std::string name, std::string file, std::string type,
			std::string flag, bool delayLoad,std::string &err_msg);
	KSingleAcserver * getSingleAcserver(std::string table_name);
#ifdef ENABLE_MULTI_SERVER
	KMultiAcserver * getMultiAcserver(std::string table_name);
	std::map<std::string,KMultiAcserver *> mservers;
	KMultiAcserver *cur_mserver;
#endif
	KPoolableRedirect *getAcserver(std::string table_name);
	KCgiRedirect *getCgiRedirect(std::string name);
	KApiRedirect *getApiRedirect(std::string name);
	KExtendProgram *cur_extend;
#ifdef ENABLE_VH_RUN_AS
	KCmdPoolableRedirect *cur_cmd;
	KCmdPoolableRedirect *getCmdRedirect(std::string name);
	std::map<std::string,KCmdPoolableRedirect *> cmds;	
#endif
	std::map<std::string,KSingleAcserver *> acservers;
	std::map<std::string,KCgiRedirect *> cgis;
	std::map<std::string,KApiRedirect *> apis;
	KRWLock lock;

};
#endif /*KACSERVERMANAGER_H_*/
