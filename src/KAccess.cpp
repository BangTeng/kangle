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
#include <string.h>
#include <stdlib.h>
#include "KAccess.h"
#include <map>
#include "KChain.h"
#include "whm.h"
#include "WhmContext.h"
#include "KAcserverManager.h"
#include "KWriteBackManager.h"
#include "KBaseVirtualHost.h"
#include "http.h"
#include "do_config.h"
#include "KTable.h"
#include "malloc_debug.h"
#include "cache.h"
#include "KSrcAcl.h"
#include "KModelManager.h"
#include "KPathAcl.h"
#include "KRegPathAcl.h"
#include "KUrlAcl.h"
#include "KSpeedLimitMark.h"
#include "KGSpeedLimitMark.h"
#include "KFlagMark.h"
#include "KHostAcl.h"
#include "KSelfPortAcl.h"
#include "KDstPortAcl.h"
#include "KMethodAcl.h"
#include "KRewriteMarkEx.h"
#include "KObjFlagAcl.h"
#include "KSelfIpAcl.h"
#include "KRequestHeaderAcl.h"
#include "KResponseHeaderAcl.h"
#include "KRegContentMark.h"
#include "KContentLengthAcl.h"
#include "KResponseFlagMark.h"
#include "KLoadAvgAcl.h"
#include "KFileAcl.h"
#include "KDirAcl.h"
#include "KFileExeAcl.h"
#include "KTimeAcl.h"
#include "KHttpProxyFetchObject.h"
#include "KVirtualHost.h"
#include "KNsVirtualHost.h"
#include "KVirtualHostManage.h"
#include "KFastcgiFetchObject.h"
#include "KRewriteMark.h"
#include "KCacheControlMark.h"
#include "KRedirectMark.h"
#include "KAuthMark.h"
#include "KMultiHostAcl.h"
#include "KSSLSerialAcl.h"
#include "KCdnMysqlMark.h"
#include "KCdnRewriteMark.h"
#include "KAuthUserAcl.h"
#include "KRefererAcl.h"
#include "KRegFileAcl.h"
#include "KSelector.h"
#include "KAddHeaderMark.h"
#include "KRemoveHeaderMark.h"
#include "KReplaceHeaderMark.h"
#include "KFooterMark.h"
#include "KReplaceIPMark.h"
#include "KReplaceContentMark.h"
#include "KSrcsAcl.h"
#include "KSelfsAcl.h"
#include "KSelfPortsAcl.h"
#include "KIpSpeedLimitMark.h"
#include "KHttp10Mark.h"
#include "KRandAcl.h"
#include "KCloudIpAcl.h"
#ifdef ENABLE_INPUT_FILTER
#include "KParamMark.h"
#include "KPostFileMark.h"
#include "KHttpOnlyCookieMark.h"
#include "KUploadProgressMark.h"
#endif
#include "KTempFileMark.h"
#include "KRemoveParamMark.h"
#include "KHostAliasMark.h"
#include "KFlowMark.h"
#include "KUrlRewriteMark.h"
#include "KUrlRangeMark.h"
#include "KVaryMark.h"

#include "KQueueMark.h"
#include "KPathSignMark.h"
#include "KStubStatusMark.h"
#include "KMarkMark.h"
#include "KMarkAcl.h"
#include "KCounterMark.h"
#include "KStatusCodeAcl.h"
#include "KStatusCodeMark.h"
#include "KPerIpAcl.h"
#include "KTimeoutMark.h"
#include "KKeepConnectionAcl.h"
#include "KConnectionCloseMark.h"
#include "KMinObjVerifiedMark.h"
#include "KTryFileAcl.h"
#include "KMapRedirectMark.h"
#include "malloc_debug.h"
#ifdef ENABLE_TCMALLOC
#include "google/heap-checker.h"
#endif
using namespace std;

KAccess kaccess[2];
std::map<std::string,KAcl *> KAccess::aclFactorys[2];
std::map<std::string,KMark *> KAccess::markFactorys[2];
std::map<std::string,KModel *> KAccess::runtimeModels;
KMutex KAccess::runtimeLock;
KAccess::KAccess() {
	default_jump = NULL;
	default_jump_type = JUMP_ALLOW;
	curTable = NULL;
	begin = NULL;
	postMap = NULL;
	string err_msg;
	newTable(BEGIN_TABLE, err_msg);
	check_time = 0;
	globalFlag = false;
	qName = "";
	actionParsed = false;
}
KAccess::~KAccess() {
	inter_destroy();
}
void KAccess::inter_destroy()
{
	std::map<std::string, KTable *>::iterator it;
	for (it = tables.begin(); it != tables.end(); it++) {
		(*it).second->release();
	}
	tables.clear();
	begin = NULL;
	postMap = NULL;
	if (this->default_jump) {
		this->default_jump->release();
		this->default_jump = NULL;
	}
}
void KAccess::destroy() {
	lock.WLock();
	inter_destroy();
	lock.WUnlock();
}
int KAccess::getType(int type) {
	if (type < 0 || type > 1) {
		return 0;
	}
	return type;
}
bool KAccess::isGlobal() {
	return globalFlag;
}
bool KAccess::addAclModel(u_short type,KAcl *m)
{
	if (type>1) {
		m->addRef();
		for (u_short i=0;i<2;i++) {
			aclFactorys[i].insert(std::pair<std::string,KAcl *>(m->getName(),m));
		}
		return true;
	}
	aclFactorys[type].insert(std::pair<std::string,KAcl *>(m->getName(),m));
	return true;
}
bool KAccess::addMarkModel(u_short type,KMark *m)
{
	if (type>1) {
		m->addRef();
		for (u_short i=0;i<2;i++) {
			markFactorys[i].insert(std::pair<std::string,KMark *>(m->getName(),m));
		}
		return true;
	}
	markFactorys[type].insert(std::pair<std::string,KMark *>(m->getName(),m));
	return true;
}
void KAccess::loadModel() {
#ifdef ENABLE_TCMALLOC
	HeapLeakChecker::Disabler disabler;
#endif
	addAclModel(REQUEST_RESPONSE,new KUrlAcl());
	KAcl *acl = NULL;
	addAclModel(REQUEST_RESPONSE,new KRegPathAcl());
	addAclModel(REQUEST_RESPONSE,new KRegParamAcl());
	addAclModel(REQUEST_RESPONSE,new KPathAcl());
	addAclModel(REQUEST_RESPONSE,new KDstPortAcl());
	addAclModel(REQUEST_RESPONSE,new KMethodAcl());
	addAclModel(REQUEST_RESPONSE,new KSrcAcl());

	addAclModel(REQUEST_RESPONSE,new KSrcsAcl());

	addAclModel(REQUEST,new KRequestHeaderAcl());
	addAclModel(REQUEST,new KHostAcl());
	addAclModel(REQUEST,new KWideHostAcl());
	addAclModel(REQUEST,new KMultiHostAcl());
#ifdef ENABLE_SIMULATE_HTTP
	addAclModel(REQUEST_RESPONSE, new KCloudIpAcl());
#endif
#ifndef _WIN32
	addAclModel(REQUEST_RESPONSE,new KLoadAvgAcl());
#endif
	addAclModel(REQUEST,new KTimeAcl());

	acl = new KFileExeAcl();
	addAclModel(REQUEST,acl);
	acl->addRef();
	addAclModel(RESPONSE,acl);
	acl = new KFileAcl();
	//addAclModel(REQUEST,acl);
	//acl->addRef();
	addAclModel(RESPONSE,acl);
	addAclModel(RESPONSE,new KFileNameAcl());
	addAclModel(RESPONSE,new KDirAcl());
	addAclModel(RESPONSE,new KRegFileAcl());
	addAclModel(RESPONSE,new KRegFileNameAcl());
	addMarkModel(REQUEST,new KSpeedLimitMark());
#ifdef ENABLE_REQUEST_QUEUE
	addMarkModel(REQUEST, new KQueueMark());
	addMarkModel(REQUEST, new KPerQueueMark());
#endif
	addMarkModel(REQUEST,new KGSpeedLimitMark());
	addMarkModel(REQUEST,new KRemoveParamMark());
	addMarkModel(REQUEST,new KHostAliasMark());
	addMarkModel(REQUEST, new KMinObjVerifiedMark());
	addMarkModel(REQUEST_RESPONSE,new KFlagMark());
	addMarkModel(REQUEST,new KRewriteMark());
	addMarkModel(REQUEST,new KRedirectMark());
	addMarkModel(REQUEST, new KMapRedirectMark());
	addMarkModel(REQUEST_RESPONSE,new KHttp10Mark());
	addMarkModel(REQUEST_RESPONSE,new KCounterMark());
	addMarkModel(REQUEST_RESPONSE,new KAuthMark());
	addMarkModel(REQUEST,new KExtendFlagMark());
#ifdef ENABLE_TF_EXCHANGE
	addMarkModel(REQUEST,new KTempFileMark());
#endif
	addAclModel(REQUEST,new KPerIpAcl());
#ifdef KSOCKET_SSL
	addAclModel(REQUEST,new KSSLSerialAcl());
#endif
	
	addAclModel(RESPONSE,new KObjFlagAcl());
	addAclModel(RESPONSE,new KHostAcl());
	addAclModel(RESPONSE,new KWideHostAcl());
	addAclModel(RESPONSE,new KMultiHostAcl());

	addAclModel(RESPONSE,new KResponseHeaderAcl());
	addAclModel(RESPONSE,new KContentLengthAcl());
	addAclModel(RESPONSE,new KStatusCodeAcl());
	addMarkModel(RESPONSE,new KCacheControlMark());
	addMarkModel(RESPONSE, new KGuestCacheMark());
	addMarkModel(RESPONSE,new KRegContentMark());
	addMarkModel(RESPONSE,new KResponseFlagMark());
	addMarkModel(RESPONSE,new KExtendFlagMark());
	addMarkModel(RESPONSE,new KStatusCodeMark());

	acl = new KMarkAcl();
	acl->addRef();
	addAclModel(REQUEST,acl);
	addAclModel(RESPONSE,acl);

	acl = new KSelfIpAcl();
	addAclModel(REQUEST,acl);
	acl->addRef();
	addAclModel(RESPONSE,acl);
	acl = new KSelfsAcl();
	addAclModel(REQUEST,acl);
	acl->addRef();
	addAclModel(RESPONSE,acl);

	acl = new KSelfPortAcl();
	addAclModel(REQUEST,acl);
	acl->addRef();
	addAclModel(RESPONSE,acl);
	acl = new KSelfPortsAcl();
	addAclModel(REQUEST,acl);
	acl->addRef();
	addAclModel(RESPONSE,acl);	
	//acl = new KVhAcl();
	//addAclModel(REQUEST,acl);
	//acl->addRef();
	//addAclModel(RESPONSE,acl);
	addAclModel(REQUEST_RESPONSE,new KRandAcl());
	addAclModel(REQUEST_RESPONSE,new KAuthUserAcl());
	addAclModel(REQUEST_RESPONSE,new KRegAuthUserAcl());
	addAclModel(REQUEST,new KRefererAcl());
	addAclModel(REQUEST,new KTryFileAcl());
	addMarkModel(REQUEST,new KRewriteMarkEx());
	addMarkModel(REQUEST,new KUrlRewriteMark());
	addMarkModel(REQUEST,new KHostMark());
	addMarkModel(REQUEST,new KHostRewriteMark());
	addMarkModel(REQUEST,new KReplaceIPMark());
	addMarkModel(REQUEST,new KSelfIPMark());
	addMarkModel(REQUEST,new KParentMark());
#ifdef ENABLE_INPUT_FILTER
	addMarkModel(REQUEST,new KParamMark());
	addMarkModel(REQUEST,new KParamCountMark());	
	addMarkModel(REQUEST,new KPostFileMark());
	addMarkModel(RESPONSE,new KHttpOnlyCookieMark());
	addMarkModel(RESPONSE,new KCookieMark());
#endif	
	
	addMarkModel(REQUEST,new KPathSignMark());
	addAclModel(REQUEST_RESPONSE,new KListenPortsAcl());
	addAclModel(REQUEST_RESPONSE,new KKeepConnectionAcl());
	addMarkModel(REQUEST,new KFlowMark());
	addMarkModel(REQUEST,new KVaryMark());	
	addMarkModel(REQUEST,new KIpSpeedLimitMark());
	addMarkModel(REQUEST_RESPONSE,new KAddHeaderMark());
	addMarkModel(REQUEST_RESPONSE,new KRemoveHeaderMark());
	addMarkModel(REQUEST_RESPONSE,new KReplaceHeaderMark());
	addMarkModel(REQUEST_RESPONSE,new KTimeoutMark());
	addMarkModel(RESPONSE,new KFooterMark());
	addMarkModel(RESPONSE,new KReplaceContentMark());
	addMarkModel(REQUEST,new KUrlRangeMark());
	addMarkModel(REQUEST,new KMarkMark());
#ifdef ENABLE_STAT_STUB
	addMarkModel(REQUEST,new KStubStatusMark());
#endif
	addMarkModel(RESPONSE,new KMarkMark());
	addMarkModel(REQUEST_RESPONSE,new KConnectionCloseMark());
}
int KAccess::checkPostMap(KHttpRequest *rq,KHttpObject *obj)
{
	if (postMap==NULL) {
		return JUMP_ALLOW;
	}
	register int jumpType = default_jump_type;
	unsigned checked_table = 0;
	KJump *jump = default_jump;
	const char *hitTable = NULL;
	int hitChain;
	lock.RLock();
	if (postMap) {
		if (postMap->match(rq, obj, jumpType, &jump, checked_table, &hitTable,&hitChain) 
				&& jumpType != JUMP_RETURN) {			
		} else {
			jumpType = default_jump_type;
		}
	}
	lock.RUnlock();
	if (jumpType==JUMP_ALLOW) {
		return JUMP_ALLOW;
	}
	return JUMP_DENY;
}
int KAccess::check(KHttpRequest *rq, KHttpObject *obj) {
	register int jumpType = default_jump_type;
	unsigned checked_table = 0;
	KJump *jump = default_jump;
	KPoolableRedirect *as;
	const char *hitTable = NULL;
	int hitChain;
	lock.RLock();
	if (!actionParsed) {
		setChainAction();
	}
	if (begin) {
		if (begin->match(rq, obj, jumpType, &jump, checked_table, &hitTable,&hitChain)
			&& jumpType != JUMP_RETURN) {
		} else {
			//reset jump to default
			jumpType = default_jump_type;
			jump = default_jump;
		}
	}
	switch (jumpType) {
	case JUMP_SERVER:
	{
		assert(rq->fetchObj == NULL);
		as = (KPoolableRedirect *)jump;
		//if (as->proto != Proto_http && as->proto!=Proto_ajp && as->proto!=Proto_tcp) {
		//	jumpType = JUMP_DENY;
		//} else {
		assert(rq->fetchObj == NULL);
		rq->fetchObj = as->makeFetchObject(rq, NULL);
		as->addRef();
		KBaseRedirect *brd = new KBaseRedirect(as, 0);
		rq->fetchObj->bindBaseRedirect(brd);
		brd->release();
		jumpType = JUMP_ALLOW;
		//}
		break;
	}
#ifdef ENABLE_WRITE_BACK
	case JUMP_WBACK:
		if (jump) {
			KWriteBack *wb = (KWriteBack *) jump;
			wb->buildRequest(rq);
		}
		jumpType = JUMP_DENY;
		break;
#endif
	case JUMP_PROXY:
		assert(rq->fetchObj==NULL);
#ifdef HTTP_PROXY
		if (rq->meth == METH_CONNECT) 
			rq->fetchObj = new KConnectProxyFetchObject();
		else 
#endif
		rq->fetchObj = new KHttpProxyFetchObject();
		break;
	}
	lock.RUnlock();
	return jumpType;
}
KTable * KAccess::getTable(std::string table_name) {
	std::map<std::string,KTable *>::iterator it = tables.find(table_name);
	if (it!=tables.end()) {
		return (*it).second;
	}
	return NULL;
}
bool KAccess::newTable(std::string table_name, std::string &err_msg) {
	bool result = false;

	if (table_name != "") {
		if (table_name.size() < 2 || table_name.size() > 16) {
			err_msg = LANG_TABLE_NAME_LENGTH_ERROR;
			return false;
		}
	}
	KTable *m_table = NULL;
	lock.WLock();
#ifndef ENABLE_MULTI_TABLE
	if (tables.size() > 0) {
		goto done;
	}
#endif
	if (getTable(table_name) != NULL) {
		goto done;
	}
	m_table = new KTable();
	m_table->name = table_name;
	if (table_name == BEGIN_TABLE && begin == NULL) {
		begin = m_table;
	} else {
		if (type==RESPONSE && table_name=="POSTMAP" && postMap==NULL) {
			postMap = m_table;
		}
	}
	tables.insert(std::pair<std::string,KTable *>(table_name,m_table));
	result = true;
	done: lock.WUnlock();
	if (!result) {
		err_msg = LANG_TABLE_NAME_IS_USED;
	}
	return result;
}
bool KAccess::delTable(std::string table_name, std::string &err_msg) {
	err_msg = LANG_TABLE_NAME_ERR;
	lock.WLock();
	std::map<std::string,KTable *>::iterator it = tables.find(table_name);
	if (it==tables.end()) {
		lock.WUnlock();
		return false;
	}
	if ((*it).second->getRef()>1) {
		err_msg = LANG_TABLE_REFS_ERR;
		lock.WUnlock();
		return false;
	}
	if ((*it).second->head) {
		err_msg = LANG_TABLE_NOT_EMPTY;
		lock.WUnlock();
		return false;
	}
	if (begin==(*it).second || postMap==(*it).second) {
		//system table cann't delete
		err_msg = LANG_TABLE_NAME_ERR;
		lock.WUnlock();
		return true;
	}
	(*it).second->release();
	tables.erase(it);
	lock.WUnlock();
	return true;
}
void KAccess::startXml(const std::string &encoding) {
	//printf("encoding=[%s]\n",encoding.c_str());
	actionParsed = false;
	lock.WLock();
}
void KAccess::endXml(bool result) {
	lock.WUnlock();
}
void KAccess::copy(KAccess &a)
{
	lock.WLock();
	default_jump_type = a.default_jump_type;
	jump_name = a.jump_name;
	if (default_jump) {
		default_jump->release();
	}
	default_jump = a.default_jump;
	a.default_jump = NULL;
	begin = a.begin;
	a.begin = NULL;
	postMap = a.postMap;
	a.postMap = NULL;
	tables.swap(a.tables);
	actionParsed = a.actionParsed;
	lock.WUnlock();
}
bool KAccess::startElement(KXmlContext *context, std::map<std::string,
		std::string> &attribute) {
	if (context == NULL)
		return false;
	bool result = false;
	if (context->qName == qName) {
		if (attribute["action"].size()>0) {
			parseChainAction(attribute["action"], default_jump_type, jump_name);
		}
		check_time = atoi(attribute["check_time"].c_str());
	}
	if (context->getParentName() == qName && context->qName == "table") {
		assert(curTable==NULL);
		curTable = getTable(attribute["name"]);
		if (curTable == NULL) {
			curTable = new KTable();
			curTable->name = attribute["name"];
			if (curTable->name.size() == 0) {
				klog(KLOG_ERR, "Warning: table name is empty in %s.\n",
						context->getPoint().c_str());
			}
			if (curTable->name == BEGIN_TABLE && begin == NULL) {
				begin = curTable;
			} else {
				if (type==RESPONSE && curTable->name == "POSTMAP" && postMap==NULL) {
					postMap = curTable;
				}
			}
			tables.insert(std::pair<std::string,KTable *>(curTable->name,curTable));
		} else {
			if (!cur_config_ext) {
				//内置的优先
				curTable->ext = cur_config_ext;
			}
		}
		result = true;
	}
	if (curTable) {
		result = curTable->startElement(context, attribute,this);
	}
	return result;
}
bool KAccess::startCharacter(KXmlContext *context, char *character, int len) {
	if (curTable == NULL) {
		return false;
	}
	bool result = curTable->startCharacter(context, character, len);
	return result;

}
void KAccess::setChainAction() {
	actionParsed = true;
	std::map<std::string,KTable *>::iterator it;
	for (it = tables.begin(); it != tables.end(); it++) {
		KTable *tb = (*it).second;
		KChain *chain = tb->head;
		while (chain) {
			setChainAction(chain->jumpType, &chain->jump, chain->jumpName);
			chain = chain->next;
		}
	}
	if (this->default_jump == NULL) {
		setChainAction(this->default_jump_type, &this->default_jump,
				this->jump_name);
	}
}
bool KAccess::endElement(KXmlContext *context) {
	bool result = false;
	if (context->qName == "table" && context->getParentName() == qName) {
		curTable = NULL;
		return true;
	}
	if (context->qName == qName && context->getParentName() == "config") {

	}
	if (curTable) {
		result = curTable->endElement(context);
	}
	return result;
}
void KAccess::buildXML(std::stringstream &s, int flag) {
	lock.RLock();
	if (!actionParsed) {
		setChainAction();
	}
	s << "\t<" << qName;
	buildChainAction(default_jump_type, default_jump, s);
	if (check_time>0) {
		s << " check_time='" << check_time << "'";
	}
	s << ">\n";
	std::map<std::string,KTable *>::iterator it;
	for (it = tables.begin(); it != tables.end(); it++) {
		(*it).second->buildXML(s,(CHAIN_XML_DETAIL|flag));
	}
	lock.RUnlock();
	s << "\t</" << qName << ">\n";
}
std::string KAccess::htmlAccess(const char *vh) {
	std::stringstream s;
	if(*vh=='\0'){
		s << "<html><LINK href=/main.css type='text/css' rel=stylesheet>\n";
		s << "<body>";
	}
	s << "<script language=javascript>\n";
	s << "function show(url) { \n";
	s
			<< "window.open(url,'','height=210,width=450,resize=no,scrollbars=no,toolsbar=no,top=200,left=200');\n";
	s << "}"
		"function tableadd() {"
		"	tbl = prompt('" << LANG_INPUT_TABLE_MSG << ":','" << LANG_DEFAULT_INPUT_TABLE_NAME << "');"
		"	if(tbl==null){ return;}"
		"	window.location='tableadd?vh=" << vh << "&access_type=" << type << "&table_name=' + tbl;"
		"}"
		"function tablerename(access_type,name_from){"
		"	tbl = prompt('" << LANG_RENAME_TABLE_INPUT_MSG << "',name_from);"
		"	if(tbl==null){ return; }"
		"	window.location='tablerename?vh=" << vh << "&access_type='+access_type+'&name_from='+name_from+'&name_to='+tbl;"
		"}"
		"</script>";

	s << "<form action='/accesschangefirst?access_type=" << type << "&vh=" << vh
			<< "' method=post name=accessaddform>" ;
	s << (type==REQUEST ? klang["lang_requestAccess"] : klang["lang_responseAccess"]) << " " << LANG_ACCESS_FIRST << ":";
	lock.WLock();
	if (!actionParsed) {
		setChainAction();
	}

	//bool show[]= {true,false,false,true,true,false,false};
	htmlChainAction(s, default_jump_type, default_jump, false, "");
	s << "<input type=submit value='" << LANG_CHANGE_FIRST_ACCESS
			<< "'></form>";
#ifdef ENABLE_MULTI_TABLE
	s << "[<a href=\"javascript:tableadd()\">" << LANG_ADD
			<< LANG_TABLE << "</a>]<br><br>";
#endif
	std::map<std::string,KTable *>::iterator it;
	for (it = tables.begin(); it != tables.end(); it++) {
		(*it).second->htmlTable(s,vh,type);
		s << "<br>";
	}
	lock.WUnlock();
	if(*vh=='\0'){
		s << endTag();
	}
	//s << "<hr><center>" << PROGRAM_NAME << "(" << VER_ID << ")</center>";
	return s.str();
}
bool KAccess::parseChainAction(std::string action, int &jumpType,
		std::string &jumpName) {
	if (strcasecmp(action.c_str(), "deny") == 0) {
		jumpType = JUMP_DENY;
	}
	if (strcasecmp(action.c_str(), "drop") == 0) {
		jumpType = JUMP_DROP;
	}
	if (strcasecmp(action.c_str(), "allow") == 0) {
		jumpType = JUMP_ALLOW;
	}
	if (strcasecmp(action.c_str(), "continue") == 0) {
		jumpType = JUMP_CONTINUE;
	}
	if (strcasecmp(action.c_str(), "return") == 0 || strcasecmp(action.c_str(), "default") == 0) {
		jumpType = JUMP_RETURN;
	}
	if (strncasecmp(action.c_str(), "table:", 6) == 0) {
		jumpType = JUMP_TABLE;
		jumpName = action.substr(6);
	}
#if 0
	if (strncasecmp(action.c_str(), "tablechain:", 11) == 0) {
		jumpType = JUMP_TABLECHAIN;
		jumpName = action.substr(11);
	}
#endif
	if (strncasecmp(action.c_str(), "wback:", 6) == 0) {
		jumpType = JUMP_WBACK;
		jumpName = action.substr(6);
	}
	if (strcasecmp(action.c_str(), "proxy") == 0) {
		jumpType = JUMP_PROXY;
	}	
	if (strncasecmp(action.c_str(), "server:", 7) == 0) {
		jumpType = JUMP_SERVER;
		jumpName = action.substr(7);
	}
	//user access not support this action
	if (isGlobal()) {
		if (strncasecmp(action.c_str(), "vhs", 4) == 0) {
			jumpType = JUMP_VHS;
		}
		if (strncasecmp(action.c_str(), "cgi:", 4) == 0) {
			jumpType = JUMP_CGI;
			jumpName = action.substr(4);
		}
		if (strncasecmp(action.c_str(), "api:", 4) == 0) {
			jumpType = JUMP_API;
			jumpName = action.substr(4);
		}
		if (strncasecmp(action.c_str(), "cmd:", 4) == 0) {
			jumpType = JUMP_CMD;
			jumpName = action.substr(4);
		}
	}
	return true;
}
void KAccess::buildChainAction(int jumpType, KJump *jump, std::stringstream &s) {
	bool jname = false;
	s << " action='";
	switch (jumpType) {
	case JUMP_DROP:
		s << "drop";
		break;
	case JUMP_DENY:
		s << "deny";
		break;
	case JUMP_ALLOW:
		s << "allow";
		break;
	case JUMP_CONTINUE:
		s << "continue";
		break;
	case JUMP_RETURN:
		s << "return";
		break;
	case JUMP_PROXY:
		s << "proxy";
		break;
#ifndef HTTP_PROXY
	case JUMP_VHS:
		s << "vhs";
		//jname = true;
		break;
#endif
	case JUMP_SERVER:
		s << "server:";
		jname = true;
		break;
	case JUMP_WBACK:
		s << "wback:";
		jname = true;
		break;
	case JUMP_TABLE:
		s << "table:";
		jname = true;
		break;
	}
	if (jname && jump) {
		s << jump->name;
	}
	s << "' ";
}
void KAccess::setChainAction(int &jump_type, KJump **jump, std::string name) {
	if (*jump) {
		(*jump)->release();
	}
	switch (jump_type) {
#ifdef ENABLE_MULTI_TABLE
	case JUMP_TABLE:
		if (name[0]=='~') {
			*jump = (KJump *) kaccess[type].getTable(name);
		} else {
			*jump = (KJump *) getTable(name);
		}
		if (*jump == NULL) {
			fprintf(stderr, "cann't get table name=[%s]\n", name.c_str());
			jump_type = JUMP_DENY;
		} else {
			(*jump)->addRef();
		}
		break;	
#if 0
	case JUMP_TABLECHAIN: {
		char *name2 = xstrdup(name.c_str());
		char *p = strchr(name2, ':');
		std::string chainname;
		if (p) {
			*p = 0;
			chainname = p + 1;
		}
		KTable *jtable = getTable(name2);
		if (jtable) {
			int id = jtable->getChain(chainname.c_str());
			if (id >= 0) {
				KJumpTable *jjtable = new KJumpTable(jtable);				
				jjtable->id = id;
				*jump = jjtable;
			}
		}
		if (*jump == NULL) {
			jump_type = JUMP_DENY;
		}
	}
		break;
#endif
#endif
	case JUMP_SERVER:
		*jump = (KJump *) conf.gam->refsAcserver(name);
		if (*jump == NULL) {
			klog(KLOG_ERR, "cann't get server name=[%s]\n", name.c_str());
			jump_type = JUMP_DENY;
		}
		break;
		/*
		 case JUMP_VHS:
		 *jump = conf.gvm->refsNsVirtualHost(name);
		 if (*jump == NULL) {
		 klog(KLOG_ERR, "cann't get virtualHost name=[%s]\n", name.c_str());
		 jump_type = JUMP_DENY;
		 }
		 break;
		 */
#ifdef ENABLE_WRITE_BACK
	case JUMP_WBACK:
		*jump = (KJump *) writeBackManager.refsWriteBack(name);
		if (*jump == NULL) {
			klog(KLOG_ERR, "cann't get writeback name=[%s]\n", name.c_str());
			jump_type = JUMP_DENY;
		}
		break;
#endif

	default:
		*jump = NULL;
	}

	return;
}
void KAccess::changeFirst(int jump_type, std::string name) {

	lock.WLock();
	default_jump_type = jump_type;
	setChainAction(default_jump_type, &default_jump, name);
	lock.WUnlock();
}
void KAccess::htmlChainAction(std::stringstream &s, int jump_type, KJump *jump,
		bool showTable, std::string skipTable) {
	if (!actionParsed) {
		setChainAction();
	}
	int jump_value = 0;
	s << "\n<input type=radio ";
	if (jump_type == JUMP_DENY) {
		s << "checked";
	}
	s << " value='" << JUMP_DENY << "' name=jump_type>" << LANG_DENY;
	jump_value++;
	if (skipTable.size() > 0) {
		s << "<input type=radio ";
		if (jump_type == JUMP_RETURN) {
			s << "checked";
		}
		s << " value='" << JUMP_RETURN << "' name=jump_type>"
				<< klang["return"];
		jump_value++;
	}
	if (type == RESPONSE || !isGlobal()) {
		s << "\n<input type=radio ";
		if (jump_type == JUMP_ALLOW) {
			s << "checked";
		}
		s << " value='" << JUMP_ALLOW << "' name=jump_type>" << LANG_ALLOW;
		jump_value++;
	} 
	//CONTINUE
	if (showTable) {
		s << "\n<input type=radio ";
		if (jump_type == JUMP_CONTINUE) {
			s << "checked";
		}
		s << " value='" << JUMP_CONTINUE << "' name=jump_type>\n"
				<< klang["LANG_CONTINUE"];
		jump_value++;
	}
	vector<string> table_names;

#ifdef ENABLE_WRITE_BACK
	//	if (show[WRITE_BACK]) {
	table_names = writeBackManager.getWriteBackNames();
	htmlRadioAction(s, &jump_value, jump_type, jump, JUMP_WBACK, "wback",
			table_names);
	//}
#endif

	if (type == REQUEST) {
		s << "<input type=radio ";
		if (jump_type == JUMP_PROXY) {
			s << "checked";
		}
		s << " value='" << JUMP_PROXY << "' name=jump_type>" << klang["proxy"];
		jump_value++;
		if (isGlobal()) {
#ifndef HTTP_PROXY
			s << "<input type=radio ";
			if (jump_type == JUMP_VHS) {
				s << "checked";
			}
			s << " value='" << JUMP_VHS << "' name=jump_type>" << klang["vhs"];
			jump_value++;
#endif
		}
		table_names.clear();
		table_names = conf.gam->getAcserverNames(false);
		htmlRadioAction(s, &jump_value, jump_type, jump, JUMP_SERVER, "server",table_names);		
	}
	if (showTable) {
		table_names = getTableNames(skipTable,false);
		if (!this->isGlobal()) {
			vector<string> gtable_names = kaccess[type].getTableNames("",true);
			vector<string>::iterator it;
			for (it=gtable_names.begin();it!=gtable_names.end();it++) {
				table_names.push_back(*it);
			}
		}
		htmlRadioAction(s, &jump_value, jump_type, jump, JUMP_TABLE, "table",
				table_names);
	}
}

void KAccess::htmlRadioAction(std::stringstream &s, int *jump_value,
		int jump_type, KJump *jump, int my_jump_type, std::string my_type_name,
		std::vector<std::string> table_names) {
	if (table_names.size() > 0) {
		s << "<input type=radio ";
		if (jump_type == my_jump_type) {
			s << "checked";
		}
		s << " value='" << my_jump_type << "' name=jump_type>\n"
				<< klang[my_type_name.c_str()];
		s << "<select name=" << my_type_name
				<< " onclick='javascript:accessaddform.jump_type["
				<< *jump_value;
		s << "].checked=true;' \nonChange='javascript:accessaddform.jump_type["
				<< *jump_value << "].checked=true;'>\n";
		for (size_t i = 0; i < table_names.size(); i++) {
			//if (skipTable == table_names[i])
			//	continue;
			s << "<option ";
			if (jump_type == my_jump_type && jump && jump->name == table_names[i]) {
				s << "selected";
			}
			s << " value='" << table_names[i] << "'>" << table_names[i]
					<< "</option>\n";
		}
		s << "</select>\n";
		(*jump_value)++;
	}
}
bool KAccess::renameTable(std::string name_from, std::string name_to,
		std::string &err_msg) {
	bool result = false;
	err_msg = LANG_TABLE_NAME_ERR;
	if (name_from == BEGIN_TABLE) {
		err_msg = "Cann't rename system table";
		return false;
	}
	lock.WLock();
	if (getTable(name_to) != NULL) {
		lock.WUnlock();
		err_msg = LANG_TABLE_NAME_IS_USED;
		return false;
	}
	std::map<std::string,KTable *>::iterator it = tables.find(name_from);
	if (it!=tables.end()) {
		KTable *table = (*it).second;
		tables.erase(it);
		table->name = name_to;
		tables.insert(std::pair<std::string,KTable *>(name_to,table));
		result = true;
	}
	lock.WUnlock();
	return result;
}
bool KAccess::emptyTable(std::string table_name, std::string &err_msg) {
	bool result = false;
	err_msg = LANG_TABLE_NAME_ERR;
	lock.WLock();
	KTable *tb = getTable(table_name);
	if (tb) {
		tb->empty();
		result = true;
	}
	lock.WUnlock();
	return result;
}
int KAccess::newChain(std::string table_name, int index,KUrlValue *urlValue) {
	int ret = -1;
	KChain *chain;
	lock.WLock();
	KTable *m_table = getTable(table_name);
	if (m_table == NULL) {
		fprintf(stderr, "cann't get table[%s]", table_name.c_str());
		goto done;
	}
	chain = new KChain();
	if (urlValue) {
		chain->edit(urlValue,this,false);
	}
	ret = m_table->insertChain(index,chain);
	done: lock.WUnlock();
	return ret;
}
std::string KAccess::addChainForm(const char *vh,std::string table_name, int index, bool add) {
	stringstream s;
	KChain *m_chain = NULL;
	KTable *m_table = NULL;
	//vector<KChain *>::iterator it;

	lock.WLock();
	m_table = getTable(table_name);
	if (m_table == NULL) {
		lock.WUnlock();
		return "";
	}
	if (!add) {
		m_chain = m_table->findChain(index);
	}
	if(*vh=='\0'){
		s << "<html><head><title>add "
				<< " access</title><LINK href=/main.css type='text/css' rel=stylesheet>\n";
		//s << "<script language=\"javascript\" src=\"/utils.js\"></script>\n";
		s << "</head><body>\n";
	}
	s << "<script language='javascript'>\n";
	s << "function delmodel(model,mark){\n";
	s << "if(confirm('are you sure to delete this model')){\n";
	s << "	window.location='/delmodel?vh=" << vh << "&access_type=" << type << "&table_name="
			<< table_name << "&id=" << index
			<< "&model='+model+'&mark='+mark;\n";
	s << "}\n};\n";
	s << "function downmodel(model,mark){\n";
	s << "	window.location='/downmodel?vh=" << vh << "&access_type=" << type << "&table_name="
			<< table_name << "&id=" << index
			<< "&model='+model+'&mark='+mark;\n";
	s << "};\n";
	s << "function addmodel(model,mark){\n";
	s << "if(model!=''){\n";
	//s << "  accessaddform.action='/test';\n";
	s << "	accessaddform.modelflag.value='1';\n";
	s << "	accessaddform.modelname.value=model;\n";
	s << "	accessaddform.mark.value=mark;\n";
	s << "	accessaddform.submit();\n";
	//	s << "window.location='/addmodel?access_type=" << type << "&table_name="
	//			<< table_name << "&id=" << index << "&model='+model+'&mark='+mark;";
	s << "}\n};\n";
	s << "</script>\n";
	
	s << "<form action='/editchain?access_type=" << type << "&vh=" << vh
			<< "' method=post name='accessaddform'>\n";
	s << "<input type=hidden name=modelflag value='0'>\n";
	s << "<input type=hidden name=modelname value=''>\n";
	s << "<input type=hidden name=mark value='0'>\n";
	s << "<input type=hidden name=table_name value='" << table_name << "'>\n";
	s << "<input type=hidden name=id value='" << index << "'>\n";
	s << "<input type=hidden name=add value='" << add << "'>\n";
	s << (type==REQUEST ? klang["lang_requestAccess"] : klang["lang_responseAccess"]) << " " << table_name;
	s << ":";
	int jump_type = JUMP_DENY;
	KJump *jump = NULL;
	if (m_chain) {
		jump_type = m_chain->jumpType;
		jump = m_chain->jump;
	}
	s << "<table  border=1 cellspacing=0><tr><td>" << LANG_ACTION
			<< "</td><td>";
	//	bool show[]= {true,false,true,true,true,false,false};
	htmlChainAction(s, jump_type, jump, true, m_table->name);
	s << "</td></tr>\n";
	s << m_table->addChainForm(m_chain,type);
	s << "</table>\n";
	s << "<input type=submit value=" << LANG_SUBMIT << ">";
	s
			<< "<input type=button onClick='javascript:if(confirm(\"confirm delete\")){ window.location=\"/delchain?vh=" << vh << "&access_type=";
	s << type << "&table_name=" << table_name << "&id=" << index << "\";}'";
	s << " value=" << LANG_DELETE << ">";
	s << "</form>";
	lock.WUnlock();
	return s.str();

}
bool KAccess::delChain(std::string table_name, std::string name)
{
	lock.WLock();
	KTable *table = getTable(table_name);
	if (table == NULL) {
		lock.WUnlock();
		return false;
	}
	bool result = table->delChain(name);
	lock.WUnlock();
	return result;
}
bool KAccess::downChain(std::string table_name, int index) {
	lock.WLock();
	KTable *table = getTable(table_name);
	if (table == NULL) {
		lock.WUnlock();
		return false;
	}
	bool result = table->downChain(index);
	lock.WUnlock();
	return result;
}
bool KAccess::delChain(std::string table_name, int index) {
	lock.WLock();
	KTable *table = getTable(table_name);
	if (table == NULL) {
		lock.WUnlock();
		return false;
	}
	bool result = table->delChain(index);
	lock.WUnlock();
	return result;
}
bool KAccess::editChain(std::string table_name, std::string name, KUrlValue *urlValue)
{
	lock.WLock();
	KTable *table = getTable(table_name);
	if (table == NULL) {
		lock.WUnlock();
		return false;
	}
	bool result = table->editChain(name, urlValue,this);
	lock.WUnlock();
	return result;
}
bool KAccess::editChain(std::string table_name, int index, KUrlValue *urlValue) {
	lock.WLock();
	KTable *table = getTable(table_name);
	if (table == NULL) {
		lock.WUnlock();
		return false;
	}
	bool result = table->editChain(index, urlValue,this);
	lock.WUnlock();
	return result;
}
void KAccess::clearImportConfig() {
}
bool KAccess::addAcl(std::string table_name, int index, std::string acl,
		bool mark) {
	lock.WLock();
	KTable *table = getTable(table_name);
	if (table == NULL) {
		lock.WUnlock();
		return false;
	}
	bool result = table->addAcl(index, acl, mark,this);
	lock.WUnlock();
	return result;
}
bool KAccess::downModel(std::string table_name, int index, std::string acl, bool mark)
{
	lock.WLock();
	KTable *table = getTable(table_name);
	if (table == NULL) {
		lock.WUnlock();
		return false;
	}
	bool result = table->downModel(index, acl, mark);
	lock.WUnlock();
	return result;
}
bool KAccess::delAcl(std::string table_name, int index, std::string acl,
		bool mark) {
	lock.WLock();
	KTable *table = getTable(table_name);
	if (table == NULL) {
		lock.WUnlock();
		return false;
	}
	bool result = table->delAcl(index, acl, mark);
	lock.WUnlock();
	return result;
}

std::vector<std::string> KAccess::getTableNames(std::string skipName,bool global) {
	std::vector<std::string> table_names;
	std::map<std::string,KTable *>::iterator it;
	for (it = tables.begin(); it != tables.end(); it++) {
		if ((skipName.size() == 0 || (skipName.size() > 0 && skipName != (*it).first)) && (*it).first != BEGIN_TABLE) {
			if (global) {
				if ((*it).first[0]=='~') {
					table_names.push_back((*it).first);
				}
				continue;
			}
			table_names.push_back((*it).first);
		}
	}
	return table_names;
}
void KAccess::listTable(KVirtualHostEvent *ctx)
{
	lock.RLock();
	std::map<std::string,KTable *>::iterator it;
	for(it=tables.begin();it!=tables.end();it++){
		ctx->add("table",(*it).first.c_str());
	}
	lock.RUnlock();
}
bool KAccess::listChain(std::string table_name,const char *chain_name,KVirtualHostEvent *ctx,int flag)
{
	std::stringstream s;
	lock.RLock();
	KTable *table = getTable(table_name);
	if (table==NULL) {
		lock.RUnlock();	
		ctx->setStatus("cann't find table");
		return false;
	}
	bool result = table->buildXML(chain_name,s,flag);//(detail?CHAIN_XML_DETAIL:CHAIN_XML_SHORT));
	lock.RUnlock();
	if (result) {
		ctx->add("table_info",s.str().c_str());
	}
	return result;
}
KModel *KAccess::getRunTimeModel(std::string name)
{
	std::map<std::string,KModel *>::iterator it = runtimeModels.find(name);
	if (it==runtimeModels.end()) {
		return NULL;
	}
	return (*it).second;
}
void KAccess::addRunTimeModel(KModel *m)
{
	m->addRef();
	runtimeModels.insert(std::pair<std::string,KModel *>(m->name,m));
}
void KAccess::releaseRunTimeModel(KModel *model)
{
	if (model->name.size()==0) {
		model->release();
		return;
	}
	runtimeLock.Lock();
	if (model->release()==1) {
		//没有引用了。
		std::map<std::string,KModel *>::iterator it = runtimeModels.find(model->name);
		if (it!=runtimeModels.end()) {
			assert(model==(*it).second);
			(*it).second->release();
			runtimeModels.erase(it);
		}
	}
	runtimeLock.Unlock();
}
int KAccess::whmCallRunTimeModel(std::string name,WhmContext *ctx)
{
	int ret = WHM_CALL_NOT_FOUND;
	runtimeLock.Lock();
	KModel *model = getRunTimeModel(name);
	if (model) {
		model->addRef();
	}
	runtimeLock.Unlock();
	if (model) {
		ret = model->whmCall(ctx);
		releaseRunTimeModel(model);
	}
	return ret;
}
