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
#include <vector>
#include "KSocket.h"
#include "KAcserver.h"
#include "KAcserverManager.h"
#include "KSingleAcserver.h"
#include "KSSIRedirect.h"
#include "KAccess.h"
#include "utils.h"
#include <sstream>
#include "lang.h"
#include <stdlib.h>
#include <stdio.h>
#include "malloc_debug.h"
#include "KCdnContainer.h"
#include "KSelectorManager.h"
using namespace std;
KAcserverManager::KAcserverManager() {
#ifdef ENABLE_MULTI_SERVER
	cur_mserver = NULL;
#endif
	cur_extend = NULL;
#ifdef ENABLE_VH_RUN_AS
	cur_cmd = NULL;
#endif
}

KAcserverManager::~KAcserverManager() {
	std::map<std::string,KSingleAcserver *>::iterator it;
	for (it = acservers.begin(); it != acservers.end(); it++) {
		(*it).second->release();
	}
	acservers.clear();
#ifdef ENABLE_MULTI_SERVER
	std::map<std::string,KMultiAcserver *>::iterator it2;
	for (it2 = mservers.begin(); it2 != mservers.end(); it2++) {
		(*it2).second->release();
	}
	mservers.clear();
#endif
#ifdef ENABLE_VH_RUN_AS
	std::map<std::string,KCmdPoolableRedirect *>::iterator it3;
	for (it3 = cmds.begin(); it3 != cmds.end(); it3++) {
		(*it3).second->release();
	}
	cmds.clear();
#endif
	std::map<std::string,KApiRedirect *>::iterator it4;
	for(it4=apis.begin();it4!=apis.end();it4++){
		(*it4).second->release();
	}
	apis.clear();
	std::map<std::string,KCgiRedirect *>::iterator it5;
	for(it5=cgis.begin();it5!=cgis.end();it5++){
		(*it5).second->release();
	}
	cgis.clear();
	if (cur_mserver) {
		cur_mserver->release();
	}
#ifdef ENABLE_VH_RUN_AS
	if (cur_cmd) {
		cur_cmd->release();
	}
#endif
}
#ifdef ENABLE_VH_RUN_AS
void KAcserverManager::refreshCmd(time_t nowTime) {

	lock.RLock();
	std::map<std::string,KCmdPoolableRedirect *>::iterator it;
	for (it = cmds.begin(); it != cmds.end(); it++) {
		KProcessManage *pm = (*it).second->getProcessManage();
		if (pm) {
			pm->refresh(nowTime);
		}
	}
	lock.RUnlock();

}
void KAcserverManager::killAllProcess(KVirtualHost *vh) {

	spProcessManage.killAllProcess(vh);
	lock.RLock();
	std::map<std::string,KCmdPoolableRedirect *>::iterator it;
	for (it = cmds.begin(); it != cmds.end(); it++) {
		KProcessManage *pm = (*it).second->getProcessManage();
		if (pm) {
			pm->killAllProcess(vh);
		}
	}
	lock.RUnlock();

}
void KAcserverManager::killCmdProcess(USER_T user) {

	spProcessManage.killProcess2(user,0);
	lock.RLock();
	std::map<std::string,KCmdPoolableRedirect *>::iterator it;
	for (it = cmds.begin(); it != cmds.end(); it++) {
		KProcessManage *pm = (*it).second->getProcessManage();
		if (pm) {
			pm->killProcess2(user,0);
		}
	}
	lock.RUnlock();

}

void KAcserverManager::getProcessInfo(std::stringstream &s) {
	lock.RLock();
	std::map<std::string,KCmdPoolableRedirect *>::iterator it;
	for (it = cmds.begin(); it != cmds.end(); it++) {
		KProcessManage *pm = (*it).second->getProcessManage();
		if (pm) {
			pm->getProcessInfo(s);
		}
	}
	lock.RUnlock();
}
#endif
void KAcserverManager::clearImportConfig() {
	
}
#ifdef ENABLE_MULTI_SERVER
std::string KAcserverManager::macserverList(std::string name) {
	stringstream s;
	KMultiAcserver *m_a = NULL;
	std::map<std::string,KMultiAcserver *>::iterator it;
	lock.RLock();
	s << "<table border=1><tr>"
		<< "<td>" << LANG_OPERATOR << "</td>"
		<< "<td>" << LANG_NAME << "</td>"
		<< "<td>" << klang["server_type"] << "</td>"
		<< "<td>hash</td>"
		<< "<td>" << klang["cookie_stick"] << "</td>"
		<< "<td>" << klang["error_try_time"] << "</td>"
		<< "<td>" << klang["error_count"] << "</td>"
	    << "<td>" << LANG_REFS << "</td>"
		
		<< "<td>" << klang["lang_host"] << "</td>"
		<< "<td>" << klang["LANG_PORT"]	<< "</td>"
		<< "<td>" << klang["lang_life_time"] << "</td>"
		<< "<td>" << klang["lang_sock_pool_size"] << "</td>"
		<< "<td>" << klang["weight"] << "</td>"
		<< "<td>self_ip</td>"
		<< "<td>" << LANG_HIT_COUNT << "</td>"
		<< "<td>" << klang["status"] << "</td>"
		<< "</tr>";

	for (it = mservers.begin(); it != mservers.end(); it++) {
		(*it).second->getHtml(s);
		if ((*it).first == name) {
			m_a = (*it).second;
		}
    }
	s << "</table>";
	s << KMultiAcserver::form(m_a);
	lock.RUnlock();
	return s.str();
}
#endif
std::string KAcserverManager::apiList(std::string name) {
	stringstream s;
	s << "<table border=1><tr><td>" << LANG_OPERATOR << "</td><td>";
	s << LANG_NAME << "</td><td>" << klang["file"] << "</td><td>";
	s << klang["seperate_process"] << "</td><td>";
	s << LANG_REFS << "</td><td>" << LANG_STATE << "</td><td>version</td></tr>";
	lock.RLock();
	KApiRedirect *m_a = NULL;

	std::map<std::string,KApiRedirect *>::iterator it;
	for (it = apis.begin(); it != apis.end(); it++) {
		m_a = (*it).second;
		string de;
		string color = "white";
		if (m_a->enable) {
			de = "disable";
		} else {
			de = "enable";
			color = "gray";
		}
		s << "<tr bgcolor='" << color	<< "'><td>";
		if (m_a->ext) {
			s << klang["external"];
		} else {
			s << "[<a href=\"javascript:if(confirm('really delete?')){ window.location='/delapi?name=";
			s << m_a->name << "';}\">" << LANG_DELETE
					<< "</a>][<a href='/apienable?flag=" << de << "&name="
					<< m_a->name << "'>";
			s << klang[de.c_str()] << "</a>]";
			s << "[<a href='/apilist?action=edit&name=" << m_a->name << "'>"	<< LANG_EDIT << "</a>]";
		}
		s << "</td>";
		s << "<td>" << m_a->name << "</td>";
		s << "<td>" << m_a->apiFile << "</td>";
		s << "<td>" << (m_a->type == WORK_TYPE_SP ? LANG_ON : LANG_OFF)
				<< "</td>";
		s << "<td>" << m_a->getRefFast() << "</td>";
		s << "<td>" << klang[m_a->dso.getStateString()] << "</td>";
		s << "<td>" << m_a->dso.getInfo() << "</td>";
		s << "</tr>";
	}
	if (name.size() > 0) {
		m_a = getApiRedirect(name);
	} else {
		m_a = NULL;
	}
	s << "</table><br>";
	s << "<form action='/apiform?action=" << (m_a ? "edit" : "add")
			<< "' method='post'>";
	s << LANG_NAME << ": <input name='name' value='" << (m_a ? m_a->name : "")
			<< "' " << (m_a ? "readonly" : "") << "><br>\n";
	s << klang["file"] << ": <input name='file' size='80' value='" << (m_a ? m_a->apiFile
			: "") << "'><br>\n";
	s << "<input type=checkbox name='sp' value='1' ";
	if (m_a==NULL || m_a->type == WORK_TYPE_SP) {
		s << "checked";
	}
	s << ">" << klang["seperate_process"] << "<br>";
	s << "<input type='submit' value='" << LANG_SUBMIT << "'>";
	s << "</form>";
	lock.RUnlock();
	return s.str();
}
#ifdef ENABLE_VH_RUN_AS
bool KAcserverManager::cmdForm(std::map<std::string, std::string> &attribute,
		std::string &errMsg) {
	string name = attribute["name"];
	string action = attribute["action"];
	bool result = true;
	KCmdPoolableRedirect *rd = NULL;
	lock.WLock();
	if (action == "add") {
		rd = newCmdRedirect(attribute,errMsg);
		if (rd==NULL) {
			result = false;
		}else{
			
		}
	} else if (action == "edit") {
		rd = getCmdRedirect(name);
		if (rd == NULL) {
			errMsg = "cann't find cgi";
			lock.WUnlock();
			return false;
		}
		rd->parseConfig(attribute);
	}
	if(rd){
		char *env = strdup(attribute["env"].c_str());
		std::map<std::string,std::string> envs;
		buildAttribute(env,envs);
		rd->parseEnv(envs);
		free(env);
	}
	lock.WUnlock();
	return result;
}
#endif
bool KAcserverManager::cgiForm(std::map<std::string, std::string> &attribute,
		std::string &errMsg) {
	string name = attribute["name"];
	string action = attribute["action"];
	bool result = true;
	lock.WLock();
	if (action == "add") {
		KCgiRedirect *rd = getCgiRedirect(name);
		if (rd != NULL) {
			errMsg = "error name is used";
			lock.WUnlock();
			return false;
		}
		result = newCgiRedirect(name, attribute["cmd"], attribute["arg"],
				attribute["env"], attribute["split_char"], errMsg);
	} else if (action == "edit") {
		KCgiRedirect *rd = getCgiRedirect(name);
		if (rd == NULL) {
			errMsg = "cann't find cgi";
			lock.WUnlock();
			return false;
		}
		string cmd = attribute["cmd"];
		string env = attribute["env"];
		string split_char = attribute["split_char"];
		if (cmd != rd->cmd) {
			xfree(rd->cmd);
			rd->cmd = strdup(cmd.c_str());
			//TODO:cmd Ҫ��
		}
		rd->setEnv(env, split_char);
		rd->setArg(attribute["arg"]);
	}
	lock.WUnlock();
	return result;
}
bool KAcserverManager::apiForm(std::map<std::string, std::string> &attribute,
		std::string &errMsg) {
	string name = attribute["name"];
	string action = attribute["action"];
	bool result = true;
	lock.WLock();
	if (action == "add") {
		KApiRedirect *rd = getApiRedirect(name);
		if (rd != NULL) {
			errMsg = "error name is used";
			lock.WUnlock();
			return false;
		}
		result = newApiRedirect(name, attribute["file"], (attribute["sp"]
				== "1" ? "sp" : "auto"), "", false, errMsg);
	} else if (action == "edit") {
		KApiRedirect *rd = getApiRedirect(name);
		if (rd == NULL) {
			errMsg = "cann't find api";
			lock.WUnlock();
			return false;
		}
		string file = attribute["file"];
		if (attribute["sp"] == "1") {
			rd->type = WORK_TYPE_SP;
		} else {
			rd->type = WORK_TYPE_AUTO;
		}
		if (rd->apiFile != file) {
			rd->dso.detachProcess();
			rd->dso.unload();
			result = rd->load(file);
			errMsg = "cann't load api";
		}
	}
	lock.WUnlock();
	return result;
}
#ifdef ENABLE_VH_RUN_AS
std::string KAcserverManager::cmdList(std::string name) {
	stringstream s;
	s << "<script language='javascript'>"
		"function $(id) \n"
		"{ \n"
		"if (document.getElementById) \n"
		"	return document.getElementById(id); "
		"else if (document.all)\n"
		"	return document.all(id);"
		"return document.layers[id];"
		"}\n"
		 "function show_div(div_name,flag)"
		"{"
		"var el=$(div_name);"
		"if(flag)\n"
		"el.style.display='';\n"
		"else\n"
		"el.style.display='none';"
		"}"
		"function switch_mp(){ show_div('sp',false);show_div('mp',true);}"
		"function switch_sp(){ show_div('mp',false);show_div('sp',true);}"
		"</script>\n";
	s << "<table border=1><tr><td>" << LANG_OPERATOR << "</td><td>";
	s << LANG_NAME << "</td><td>" << klang["file"] << "</td><td>" << klang["type"]
		<< "</td><td>" << klang["protocol"] << "</td>"
		<< "<td>" << LANG_PORT << "</td>"
		<< "<td>" << LANG_REFS
		<< "</td><td>" << klang["env"] << "</td></tr>";
	lock.RLock();
	KCmdPoolableRedirect *m_a = NULL;

	std::map<std::string,KCmdPoolableRedirect *>::iterator it;
	for (it = cmds.begin(); it != cmds.end(); it++) {
		m_a = (*it).second;
		string de;
		string color = "white";
		if (m_a->enable) {
			de = "disable";
		} else {
			de = "enable";
			color = "gray";
		}
		s << "<tr bgcolor='" << color
				<< "'><td>";
		if (m_a->ext) {
			s << klang["external"];
		} else {
			s << "[<a href=\"javascript:if(confirm('really delete?')){ window.location='/delcmd?name=";
			s << m_a->name << "';}\">" << LANG_DELETE
					<< "</a>][<a href='/cmdenable?flag=" << de << "&name="
					<< m_a->name << "'>";
			s << klang[de.c_str()] << "</a>]";
			s << "[<a href='/extends?item=4&action=edit&name=" << m_a->name << "'>"
				<< LANG_EDIT << "</a>]";
		}
		s << "</td>";
		s << "<td>" << m_a->name << "</td>";
		s << "<td><div title='" << m_a->cmd << "'>" << m_a->cmd.substr(0,60) << "</div></td>";
		s << "<td>" << (m_a->type==WORK_TYPE_MP?klang["mp"]:klang["sp"]) ;
		if(m_a->type==WORK_TYPE_MP){
			s << "*" << m_a->worker;
		}
		s << "</td>";
		s << "<td>" << m_a->buildProto(m_a->proto) << "</td>";
		s << "<td>" << m_a->port << "</td>";
		s << "<td>" << m_a->getRefFast() << "</td>";
		s << "<td>" << m_a->getEnv() << "</td>";
		s << "</tr>";
	}
	if (name.size() > 0) {
		m_a = getCmdRedirect(name);
	} else {
		m_a = NULL;
	}
	s << "</table><br>";
	s << "<form action='/cmdform?action=" << (m_a ? "edit" : "add")
			<< "' method='post'>";
	s << LANG_NAME << ": <input name='name' value='" << (m_a ? m_a->name : "")
			<< "' " << (m_a ? "readonly" : "") << "><br>\n";
	s << klang["file"] << ": <input name='file' value='"
			<< (m_a ? m_a->cmd.c_str() : "") << "' size='80'><br>\n";
	s << klang["env"] << ":<input name='env' value='" << (m_a ? m_a->getEnv()
			: "") << "' size='80'><br>";
	//type
	const char *mp_display="display:";
	const char *sp_display="display:";
	s << klang["type"] << ":<input name='type' value='mp'  onClick='switch_mp()'  type='radio' ";
	if(m_a==NULL || m_a->type==WORK_TYPE_MP){
		s << "checked";
		sp_display="display:none";
	}
	s << ">" << klang["mp"];
	s << "<input name='type' value='sp' onClick='switch_sp()' type='radio' ";
	if(m_a && m_a->type==WORK_TYPE_SP){
		s << "checked";	
		mp_display="display:none";
	}
	s << ">" << klang["sp"];
	s << "<br>";
	s << "<div id='mp' style='" << mp_display << "'>" << klang["process_count"] << "<input name='worker' value='" << (m_a?m_a->worker:4) << "' size=4>" ;
	s << klang["error_count"] << "<input name='max_error_count' value='" << (m_a?m_a->max_error_count:0) << "' size=4></div>";
	s << "<div id='sp' style='" << sp_display << "'>" << LANG_PORT << "<input name='port' value='" << (m_a?m_a->port:0) << "' size='6'>";
#ifndef _WIN32
	s << klang["kill_sig"] << "<input name='sig' value='" << (m_a?m_a->sig:SIGKILL) << "' size='4'>";
#endif
	s << "</div>";
	//proto
	s << klang["protocol"] << ":";
	s << "<input type='radio' name='proto' value='http' ";
	if(m_a && m_a->proto == Proto_http){
		s << "checked";
	}
	s << ">http";


	s << "<input type='radio' name='proto' value='fastcgi' ";
	if(m_a && m_a->proto == Proto_fcgi){
		s << "checked";
	}
	s << ">fastcgi";

	s << "<input type='radio' name='proto' value='ajp' ";
	if(m_a && m_a->proto == Proto_ajp){
		s << "checked";
	}
	s << ">ajp";
	//uwsgi
	s << "<input type='radio' name='proto' value='uwsgi' ";
	if(m_a && m_a->proto == Proto_uwsgi){
		s << "checked";
	}
	s << ">uwsgi";
	//scgi
	s << "<input type='radio' name='proto' value='scgi' ";
	if(m_a && m_a->proto == Proto_scgi){
		s << "checked";
	}
	s << ">scgi";
	//hmux
	s << "<input type='radio' name='proto' value='hmux' ";
	if(m_a && m_a->proto == Proto_hmux){
		s << "checked";
	}
	s << ">hmux";
	
	s << "<br>";
	s << klang["lang_life_time"] << "<input name='life_time' size=5 value=" << (m_a ? m_a->lifeTime : 0) << ">";
	s << klang["lang_life_time_notice"] << "<br>";
	s << klang["idle_time"] << "<input name='idle_time' size=5 value='" << (m_a ? m_a->idleTime : 120) << "'><br>";
	s << "<input type='checkbox' name='chuser' value='0' ";
	if(m_a && !m_a->chuser){
		s << "checked";
	}
	s << ">" << klang["run_as_system"] << "<br>";

	//submit
	s << "<input type='submit' value='" << LANG_SUBMIT << "'>";
	s << "</form>";
	lock.RUnlock();
	return s.str();
}
#endif
std::string KAcserverManager::cgiList(std::string name) {
	stringstream s;
	s << "<table border=1><tr><td>" << LANG_OPERATOR << "</td><td>";
	s << LANG_NAME << "</td><td>" << klang["file"] << "</td><td>" << LANG_REFS
			<< "</td><td>" << klang["env"] << "</td></tr>";
	lock.RLock();
	KCgiRedirect *m_a = NULL;
	
	std::map<std::string,KCgiRedirect *>::iterator it;
	for (it = cgis.begin(); it != cgis.end(); it++) {
		m_a = (*it).second;
		string de;
		if (m_a->enable) {
			de = "disable";
		} else {
			de = "enable";
		}
		s << "<tr><td>[<a href=\"javascript:if(confirm('really delete?')){ window.location='/delcgi?name=";
		s << m_a->name << "';}\">" << LANG_DELETE
				<< "</a>][<a href='/cgienable?flag=" << de << "&name="
				<< m_a->name << "'>";
		s << klang[de.c_str()] << "</a>]";
		s << "[<a href='/cgilist?action=edit&name=" << m_a->name << "'>"
				<< LANG_EDIT << "</a>]";
		s << "</td>";
		s << "<td>" << m_a->name << "</td>";
		s << "<td>" << m_a->cmd << "</td>";
		s << "<td>" << m_a->getRefFast() << "</td>";
		s << "<td>" << m_a->getEnv() << "</td>";
		s << "</tr>";
	}
	if (name.size() > 0) {
		m_a = getCgiRedirect(name);
	} else {
		m_a = NULL;
	}
	s << "</table><br>";
	s << "<form action='/cgiform?action=" << (m_a ? "edit" : "add")
			<< "' method='post'>";
	s << LANG_NAME << ": <input name='name' value='" << (m_a ? m_a->name : "")
			<< "' " << (m_a ? "readonly" : "") << "><br>\n";
	s << klang["file"] << ": <input name='cmd' value='"
			<< (m_a ? m_a->cmd : "") << "'><br>\n";
	s << klang["arg"] << ": <input name='arg' value='" << (m_a ? m_a->getArg()
			: "") << "'><br>\n";
	s << klang["env"] << ":<input name='env' value='" << (m_a ? m_a->getEnv()
			: "") << "'>" << klang["split_char"]
			<< ":<input name='split_char' max_size=1 size=1 value='"
			<< (m_a ? m_a->split_char : '|') << "'><br>\n";
	s << "<input type='submit' value='" << LANG_SUBMIT << "'>";
	s << "</form>";
	lock.RUnlock();
	return s.str();
}
std::string KAcserverManager::acserverList(std::string name) {
	stringstream s;
	std::map<std::string,KSingleAcserver *>::iterator it;
	KSingleAcserver *m_a = NULL;
	s << "<table border=1><tr><td>" << LANG_OPERATOR << "</td><td>"
			<< LANG_NAME << "</td><td>" << klang["server_type"] << "</td><td>"
			<< LANG_REFS << "</td><td>" << LANG_IP << "</td><td>" << LANG_PORT << "</td>"
			<< "<td>" << klang["lang_life_time"] << "</td>"
			<< "<td>" << klang["lang_sock_pool_size"] << "</td>"
			<< "</tr>\n";
	lock.RLock();
	for (it = acservers.begin(); it != acservers.end(); it++) {
		m_a = (*it).second;
		s << "<tr><td>";
		s << "[<a href=\"javascript:if(confirm('" << LANG_CONFIRM_DELETE
				<< m_a->name;
		s << "')){ window.location='/acserverdelete?name=" << m_a->name
				<< "';}\">" << LANG_DELETE << "</a>]";
		s << "[<a href=\"/acserver?name=" << m_a->name << "\">" << LANG_EDIT
				<< "</a>]";
		s << "</td><td>" << m_a->name << "</td><td>";
		s << KPoolableRedirect::buildProto(m_a->proto);
		s << "</td><td>" << m_a->refs << "</td><td>";
		s << m_a->sockHelper->host << "</td>";
		s << "<td>" ;
		if (m_a->sockHelper->isUnix) 
			s << "-";
		else 
			s << m_a->sockHelper->port;

		s  << "</td>";
		s << "<td>" << m_a->sockHelper->getLifeTime() << "</td>";
		s << "<td>" << m_a->getPoolSize() << "</td>";

		s << "</tr>\n";
	}
	if (name.size() > 0) {
		m_a = getSingleAcserver(name);
	} else {
		m_a = NULL;
	}
	s << "</table>\n<hr>";
	s << "<form action=" << (m_a ? "/acserveredit" : "/acserveradd")
			<< " method=post>\n";
	s << LANG_NAME << ":<input name=name value='" << (m_a ? m_a->name : "")
			<< "'";
	if (m_a) {
		s << " readonly";
	}
	s << "><br>";
#ifndef HTTP_PROXY
	s << klang["protocol"] << ":";
	s << "<input type='radio' name='proto' value='http' ";
	if (m_a == NULL || m_a->proto == Proto_http) {
		s << "checked";
	}

	s << ">http <input type='radio' name='proto' value='fastcgi' ";
	if (m_a && m_a->proto == Proto_fcgi) {
		s << "checked";
	}
	s << ">fastcgi ";
	//ajp
	s << "<input type='radio' name='proto' value='ajp' ";
	if (m_a && m_a->proto == Proto_ajp) {
		s << "checked";
	}
	s << ">ajp";
	//uwsgi
	s << "<input type='radio' name='proto' value='uwsgi' ";
	if (m_a && m_a->proto == Proto_uwsgi) {
		s << "checked";
	}
	s << ">uwsgi";
	//scgi
	s << "<input type='radio' name='proto' value='scgi' ";
	if (m_a && m_a->proto == Proto_scgi) {
		s << "checked";
	}
	s << ">scgi";
	//hmux
	s << "<input type='radio' name='proto' value='hmux' ";
	if (m_a && m_a->proto == Proto_hmux) {
		s << "checked";
	}
	s << ">hmux";
	
	s << "<br>";
#endif
	s << klang["lang_host"] << "<input name='host' value='";
	if(m_a){
		s << m_a->sockHelper->host;
	}
	s << "'>" << LANG_PORT << "<input size=5 name=port value='";
	if (m_a) {
		s << m_a->sockHelper->port;

	}
	s << "'";
	s << ">\n";

	if (m_a) {
		s << "<input type=hidden name=namefrom value='" << m_a->name << "'>\n";
	}
	s << "<br>" << klang["lang_life_time"] << "<input name='life_time' size=5 value=" << (m_a ? m_a->sockHelper->getLifeTime() : 0) << ">";
	s	<< klang["lang_life_time_notice"] << "<br>";
	s << "<br>";
#ifdef HTTP_PROXY
	s << LANG_USER << ": <input name='auth_user' value='" 
		<< (m_a?m_a->sockHelper->auth_user.c_str() :"") << "'><br>";
	s << LANG_PASS << ": <input name='auth_passwd' value='" 
		<< (m_a?m_a->sockHelper->auth_passwd.c_str():"") << "'><br>";
#endif
	s << "<br><input type=submit value=" << (m_a ? LANG_EDIT : LANG_SUBMIT)
			<< "></form>\n";

	lock.RUnlock();
	return s.str();
}
#ifdef ENABLE_MULTI_SERVER
std::string KAcserverManager::macserver_node_form(std::string name,
		std::string action, unsigned nodeIndex) {
	if (action == "add") {
		return KMultiAcserver::nodeForm(name, NULL, 0);
	}
	KMultiAcserver *as = getMultiAcserver(name);
	if (as == NULL) {
		return "name is not found";
	}
	return KMultiAcserver::nodeForm(name, as, nodeIndex);
}
bool KAcserverManager::macserver_node(
		std::map<std::string, std::string> &attribute, std::string &errMsg) {
	string name = attribute["name"];
	string action = attribute["action"];
	if (name.size() == 0) {
		errMsg = "name cann't be zero";
		return false;
	}
	lock.WLock();
	KMultiAcserver *as = getMultiAcserver(name);
	if (as == NULL) {
		errMsg = "cann't find server";
		lock.WUnlock();
		return false;
	}
	
	if (action == "add" || action == "edit") {
		as->editNode(attribute);
	} else if (action == "delete") {
		as->delNode(atoi(attribute["id"].c_str()));
	}
	lock.WUnlock();
	return true;
}
#endif
void KAcserverManager::unloadAllApi()
{
	std::map<std::string,KApiRedirect *>::iterator it3;
	for (it3 = apis.begin(); it3 != apis.end(); it3++) {
		(*it3).second->dso.unload();
	}
}
std::vector<std::string> KAcserverManager::getAllTarget() {
	std::vector<std::string> targets;
	stringstream s;

	lock.RLock();
	std::map<std::string,KSingleAcserver *>::iterator it;
	for (it = acservers.begin(); it != acservers.end(); it++) {
		s.str("");
		s << (*it).second->getType() << ":" << (*it).first;
		targets.push_back(s.str());
	}
#ifdef ENABLE_MULTI_SERVER
	std::map<std::string,KMultiAcserver *>::iterator it2;
	for (it2 = mservers.begin(); it2 != mservers.end(); it2++) {
		s.str("");
		s << "server:" << (*it2).first;
		targets.push_back(s.str());
	}
#endif
	std::map<std::string,KApiRedirect *>::iterator it3;
	for (it3 = apis.begin(); it3 != apis.end(); it3++) {
		s.str("");
		s << (*it3).second->getType() << ":" << (*it3).first;
		targets.push_back(s.str());
	}

#ifdef ENABLE_VH_RUN_AS
	std::map<std::string,KCmdPoolableRedirect *>::iterator it5;
	for (it5 = cmds.begin(); it5 != cmds.end(); it5++) {
		s.str("");
		s << (*it5).second->getType() << ":" << (*it5).first;
		targets.push_back(s.str());
	}
#endif
	std::map<std::string,KCgiRedirect *>::iterator it4;
	for (it4 = cgis.begin(); it4 != cgis.end(); it4++) {
		s.str("");
		s << (*it4).second->getType() << ":" << (*it4).first;
		targets.push_back(s.str());
	}
	targets.push_back("cgi");
	targets.push_back("ssi");

	lock.RUnlock();
	return targets;
}
std::vector<std::string> KAcserverManager::getAcserverNames(bool onlyHttp) {
	std::vector<std::string> table_names;
	std::map<std::string,KSingleAcserver *>::iterator it;
	lock.RLock();
	for (it = acservers.begin(); it != acservers.end(); it++) {
		if (!onlyHttp || (onlyHttp && (Proto_http == (*it).second->proto || Proto_ajp==(*it).second->proto))) {
			table_names.push_back((*it).first);
		}
	}
#ifdef ENABLE_MULTI_SERVER
	std::map<std::string,KMultiAcserver *>::iterator it2;
	for (it2 = mservers.begin(); it2 != mservers.end(); it2++) {
		if (!onlyHttp || (onlyHttp && (Proto_http == (*it2).second->proto || Proto_ajp==(*it2).second->proto))) {
			table_names.push_back((*it2).first);
		}
	}
#endif
	lock.RUnlock();
	return table_names;
}
KSingleAcserver *KAcserverManager::refsSingleAcserver(std::string name)
{
	lock.RLock();
	KSingleAcserver *ac = getSingleAcserver(name);
	if (ac) {
		ac->addRef();
	}
	lock.RUnlock();
	return ac;
}
KPoolableRedirect *KAcserverManager::refsAcserver(std::string name) {
	lock.RLock();
	KPoolableRedirect *ac = getAcserver(name);
	if (ac) {
		ac->addRef();
	}
	lock.RUnlock();
	return ac;
}
KCgiRedirect *KAcserverManager::refsCgiRedirect(std::string name) {
	lock.RLock();
	KCgiRedirect *ac = getCgiRedirect(name);
	if (ac) {
		ac->addRef();
	}
	lock.RUnlock();
	return ac;
}
#ifdef ENABLE_MULTI_SERVER
KMultiAcserver * KAcserverManager::getMultiAcserver(std::string table_name) {
	std::map<std::string,KMultiAcserver *>::iterator it = mservers.find(table_name);
	if (it!=mservers.end()) {
		return (*it).second;
	}
	return NULL;
}
#endif
KPoolableRedirect *KAcserverManager::getAcserver(std::string table_name) {
	KPoolableRedirect *server = getSingleAcserver(table_name);
	if (server != NULL) {
		return server;
	}
#ifdef ENABLE_MULTI_SERVER
	return getMultiAcserver(table_name);
#else
	return NULL;
#endif
}
KSingleAcserver * KAcserverManager::getSingleAcserver(std::string table_name) {
	std::map<std::string,KSingleAcserver *>::iterator it;
	it = acservers.find(table_name);
	if (it!=acservers.end()) {
		return (*it).second;
	}
	return NULL;	
}
bool KAcserverManager::newSingleAcserver(
	bool overFlag,
	std::map<std::string, std::string> &attr ,
	std::string &err_msg) {
	std::string proto = attr["proto"];
	if(proto.size()==0){
		proto = attr["type"];
	}
	std::string name = attr["name"];
	bool result = false;
	if (name.size() <= 0) {
		err_msg = LANG_TABLE_NAME_LENGTH_ERROR;
		return false;
	}
	KSingleAcserver *sa = conf.gam->refsSingleAcserver(name);
	if (this == conf.gam && sa) {
		//http manage edit
		lock.WLock();
		sa->proto = KPoolableRedirect::parseProto(proto.c_str());
		sa->sockHelper->parse(attr);
		lock.WUnlock();
		sa->release();
		return true;
	}
	KSingleAcserver *m_a = new KSingleAcserver;
	m_a->name = name;
	m_a->proto = KPoolableRedirect::parseProto(proto.c_str());
	m_a->sockHelper->parse(attr);
	if (sa) {
		if (sa->isChanged(m_a)) {
			sa->release();
		} else {
			m_a->release();
			m_a = sa;
		}
	}
	lock.WLock();
	std::map<std::string,KSingleAcserver *>::iterator it = acservers.find(name);
	if (it!=acservers.end()) {
		if (!overFlag) {
			goto done;
		}
		(*it).second->release();
		acservers.erase(it);
	}
#ifdef ENABLE_MULTI_SERVER
	if (!overFlag && getMultiAcserver(name)) {
		goto done;
	}
#endif
	acservers.insert(std::pair<std::string,KSingleAcserver *>(m_a->name,m_a));
	m_a->addRef();
	result = true;
	done: 
	lock.WUnlock();
	m_a->release();
	if (!result) {
		err_msg = LANG_TABLE_NAME_IS_USED;
	}
	return result;
}
bool KAcserverManager::delApi(std::string name, std::string &err_msg) {
	err_msg = LANG_TABLE_NAME_ERR;
	lock.WLock();
	std::map<std::string,KApiRedirect *>::iterator it = apis.find(name);
	if (it==apis.end()) {
		lock.WUnlock();
		return false;
	}
	if ((*it).second->getRef()>1) {
		lock.WUnlock();
		err_msg = LANG_TABLE_REFS_ERR;
		return false;
	}
	(*it).second->release();
	apis.erase(it);
	lock.WUnlock();
	return true;
}
#ifdef ENABLE_VH_RUN_AS
bool KAcserverManager::delCmd(std::string name, std::string &err_msg) {
	err_msg = LANG_TABLE_NAME_ERR;
	lock.WLock();
	std::map<std::string,KCmdPoolableRedirect *>::iterator it = cmds.find(name);
	if (it==cmds.end()) {
		lock.WUnlock();
		return false;
	}
	if ((*it).second->getRef()>1) {
		lock.WUnlock();
		err_msg = LANG_TABLE_REFS_ERR;
		return false;
	}
	(*it).second->release();
	cmds.erase(it);
	lock.WUnlock();
	return true;
}
#endif
bool KAcserverManager::delCgi(std::string name, std::string &err_msg) {
	err_msg = LANG_TABLE_NAME_ERR;
	lock.WLock();
	std::map<std::string,KCgiRedirect *>::iterator it = cgis.find(name);
	if (it==cgis.end()) {
		lock.WUnlock();
		return false;
	}
	if ((*it).second->getRef()>1) {
		lock.WUnlock();
		err_msg = LANG_TABLE_REFS_ERR;
		return false;
	}
	(*it).second->release();
	cgis.erase(it);
	lock.WUnlock();
	return true;
}
#ifdef ENABLE_MULTI_SERVER
bool KAcserverManager::delMAcserver(std::string name, std::string &err_msg) {
	err_msg = LANG_TABLE_NAME_ERR;
	lock.WLock();
	std::map<std::string,KMultiAcserver *>::iterator it = mservers.find(name);
	if (it==mservers.end()) {
		lock.WUnlock();
		return false;
	}
	if ((*it).second->refs > 1) {
		err_msg = LANG_TABLE_REFS_ERR;
		lock.WUnlock();
		return false;
	}
	(*it).second->release();
	mservers.erase(it);
	lock.WUnlock();
	return true;
}
#endif
bool KAcserverManager::delAcserver(std::string name, std::string &err_msg) {
	err_msg = LANG_TABLE_NAME_ERR;
	lock.WLock();
	std::map<std::string,KSingleAcserver *>::iterator it = acservers.find(name);
	if (it==acservers.end()) {
		lock.WUnlock();
		return false;
	}
	if ((*it).second->refs>1) {
		err_msg = LANG_TABLE_REFS_ERR;
		lock.WUnlock();
		return false;
	}
	(*it).second->release();
	acservers.erase(it);
	lock.WUnlock();
	return true;
}
KRedirect *KAcserverManager::refsRedirect(std::string target) {
	int jumpType;
	string name;
	if (strncasecmp(target.c_str(), "cdn:",4) == 0) {
		KRedirect *rd = NULL;
		char *tmp = strdup(target.c_str() + 4);
		char *p = strchr(tmp,':');
		if (p) {
			*p = '\0';
			char *host = p+1;
			p = strchr(host,':');
			if(p){
				*p = '\0';
				int port = atoi(p+1);
				p = strchr(p+1,':');
				int life_time = 0;
				if(p){
					*p = '\0';
					life_time = atoi(p+1);
				}
				rd = cdnContainer.refsRedirect(NULL,host,port,NULL,life_time,KPoolableRedirect::parseProto(tmp));
			}
		}
		free(tmp);
		return rd;
	}
	if (strcasecmp(target.c_str(), "cgi") == 0 || strcasecmp(target.c_str(),
			"cgi:") == 0) {
		globalCgi.addRef();
		return &globalCgi;
	}
	if (strcasecmp(target.c_str(), "ssi") == 0 || strcasecmp(target.c_str(),
			"ssi:") == 0) {
		ssi.addRef();
		return &ssi;
	}
	if (!kaccess[REQUEST].parseChainAction(target, jumpType, name)) {
		debug("cann't parse target=[%s]\n", target.c_str());
		return NULL;
	}
	switch (jumpType) {
	case JUMP_MSERVER:
		return refsMultiAcserver(name);
	case JUMP_SERVER:
		return refsAcserver(name);
	case JUMP_CGI:
		return refsCgiRedirect(name);
	case JUMP_API:
		return refsApiRedirect(name);
#ifdef ENABLE_VH_RUN_AS
	case JUMP_CMD:
		return refsCmdRedirect(name);
#endif
	}
	return NULL;
}
bool KAcserverManager::newApiRedirect(std::string name, std::string file,
		std::string type, std::string flag, bool delayLoad,
		std::string &err_msg) {
	if (getApiRedirect(name)) {
		return false;
	}
	KApiRedirect *rd = new KApiRedirect();
	rd->name = name;
	if (type.size() > 0) {
		rd->type = KApiRedirect::getTypeValue(type.c_str());
	}
	if (strstr(flag.c_str(), "disable") != NULL) {
		rd->enable = false;
	}
	rd->setDelayLoad(delayLoad);
	if (this!=conf.gam) {
		KApiRedirect *sa = conf.gam->refsApiRedirect(name);
		if (sa) {
			if (sa->isChanged(rd)) {
				sa->release();
			} else {
				rd->release();
				rd = sa;
			}
		}
	}
	apis.insert(std::pair<std::string,KApiRedirect *>(rd->name,rd));
	rd->setFile(file);
	if (selectorManager.isInit()) {
		if (!rd->load()) {
			return false;
		}
	}
	cur_extend = rd;
	//	cur_extend->parseConfig(attribute);
	return true;
}
#ifdef ENABLE_VH_RUN_AS	
void KAcserverManager::loadAllApi()
{
	std::map<std::string,KApiRedirect *>::iterator it;
	lock.WLock();
	for (it=apis.begin(); it!=apis.end(); it++) {
		(*it).second->load();
	}
	lock.WUnlock();
}
#endif
bool KAcserverManager::apiEnable(std::string name, bool enable) {
	bool result = false;
	lock.WLock();
	KApiRedirect *rd = getApiRedirect(name);
	if (rd) {
		rd->enable = enable;
		result = true;
	}
	lock.WUnlock();
	return result;
}
#ifdef ENABLE_VH_RUN_AS
bool KAcserverManager::cmdEnable(std::string name, bool enable) {
	bool result = false;
	lock.WLock();
	KCmdPoolableRedirect *rd = getCmdRedirect(name);
	if (rd) {
		rd->enable = enable;
		result = true;
	}
	lock.WUnlock();
	return result;
}
#endif
bool KAcserverManager::cgiEnable(std::string name, bool enable) {
	bool result = false;
	lock.WLock();
	KCgiRedirect *rd = getCgiRedirect(name);
	if (rd) {
		rd->enable = enable;
		result = true;
	}
	lock.WUnlock();
	return result;
}
#ifdef ENABLE_VH_RUN_AS
KCmdPoolableRedirect *KAcserverManager::newCmdRedirect(
		std::map<std::string, std::string> &attribute, std::string &errMsg) {
	string name = attribute["name"];
	if (getCmdRedirect(name)) {
		errMsg = "name duplicate.";
		return NULL;
	}
	KCmdPoolableRedirect *rd = new KCmdPoolableRedirect();
	rd->name = name;
	cur_extend = rd;
	cur_extend->parseConfig(attribute);
	cmds.insert(std::pair<std::string,KCmdPoolableRedirect *>(rd->name,rd));
	return rd;
}
#endif
bool KAcserverManager::newCgiRedirect(std::string name, std::string cmd,
		std::string arg, std::string env, std::string env_split,
		std::string &err_msg) {
	if (getCgiRedirect(name)) {
		return false;
	}

	KCgiRedirect *rd = new KCgiRedirect(cmd.c_str());
	rd->name = name;
	rd->setArg(arg);
	rd->setEnv(env, env_split);
	cgis.insert(std::pair<std::string,KCgiRedirect *>(rd->name,rd));
	return true;
}

KCgiRedirect *KAcserverManager::getCgiRedirect(std::string name) {
	std::map<std::string,KCgiRedirect *>::iterator it;
	it = cgis.find(name);
	if (it!=cgis.end()) {
		return (*it).second;
	}
	return NULL;
}
#ifdef ENABLE_VH_RUN_AS
KCmdPoolableRedirect *KAcserverManager::getCmdRedirect(std::string name) {
	std::map<std::string,KCmdPoolableRedirect *>::iterator it;
	it = cmds.find(name);
	if (it!=cmds.end()) {
		return (*it).second;
	}
	return NULL;	
}
KCmdPoolableRedirect *KAcserverManager::refsCmdRedirect(std::string name) {
	lock.RLock();
	KCmdPoolableRedirect *ac = getCmdRedirect(name);
	if (ac) {
		ac->addRef();
	}
	lock.RUnlock();
	return ac;
}
#endif
KApiRedirect *KAcserverManager::refsApiRedirect(std::string name) {
	lock.RLock();
	KApiRedirect *ac = getApiRedirect(name);
	if (ac) {
		ac->addRef();
	}
	lock.RUnlock();
	return ac;
}

KApiRedirect *KAcserverManager::getApiRedirect(std::string name) {
	std::map<std::string,KApiRedirect *>::iterator it;
	it = apis.find(name);
	if (it!=apis.end()) {
		return (*it).second;
	}
	return NULL;	
}
bool KAcserverManager::startElement(std::string &context, std::string &qName,
		std::map<std::string, std::string> &attribute) {
	string errMsg;
	if (qName == "server") {
		string name = attribute["name"];
		/*
		if (getAcserver(name) != NULL) {
			fprintf(stderr, "server name=[%s] is used\n", name.c_str());
			return false;
		}
		*/
		string host = attribute["host"];
		string proto = attribute["proto"];
		if (proto.size()==0) {
			//������
			proto = attribute["type"];
		}
		if (host.size() > 0) {
			if (!newSingleAcserver(false, attribute, errMsg)) {
				fprintf(stderr, "cann't new server,errmsg=%s\n", errMsg.c_str());
			}
			return true;
		}
#ifdef ENABLE_MULTI_SERVER
		assert(cur_mserver==NULL);
		cur_mserver = new KMultiAcserver;
		cur_mserver->name = name;
		cur_mserver->parse(attribute);
#endif
		return true;
	}
#ifdef ENABLE_MULTI_SERVER
	if (qName == "node") {
		if (cur_mserver == NULL) {
			//fprintf(stderr, "node tag must under server tag\n");
			return false;
		}
		cur_mserver ->editNode(attribute);
	}
#endif
#ifndef HTTP_PROXY
	if (qName == "cgi") {
		if (!newCgiRedirect(attribute["name"], attribute["cmd"],
				attribute["arg"], attribute["env"], attribute["env_split"],
				errMsg)) {
			fprintf(stderr, "cann't new cgi,errmsg=%s\n", errMsg.c_str());
		}
		return true;
	}
	if (qName == "api") {
		string name = attribute["name"];
		string file = attribute["file"];
		bool delayLoad = false;
		if (attribute["delay_load"] == "1") {
			delayLoad = true;
		}
		if (!newApiRedirect(name, file, attribute["type"], attribute["flag"],
				delayLoad, errMsg)) {
			fprintf(stderr, "cann't load api[%s] ,errmsg=%s\n", name.c_str(),
					errMsg.c_str());
		}
		return true;
	}
	if (qName == "pre_event") {
		if (cur_extend) {
			return cur_extend->addEvent(true, attribute);
		}
	}
	if (qName == "post_event") {
		if (cur_extend) {
			return cur_extend->addEvent(false, attribute);
		}
	}
#endif
	return false;
}
bool KAcserverManager::startElement(KXmlContext *context, std::map<std::string,
			std::string> &attribute) 
{
	if(context->qName=="env" 
		&& (context->getParentName() == "api" || context->getParentName() == "cmd")) {
		if (cur_extend) {
			cur_extend->parseEnv(attribute);
		}
	}
#ifdef ENABLE_VH_RUN_AS
	if (context->qName=="cmd") {
		assert(cur_cmd==NULL);
		string name = attribute["name"];
		cur_cmd = new KCmdPoolableRedirect;
		cur_cmd->name = name;
		cur_cmd->parseConfig(attribute);
		cur_extend = cur_cmd;
	}
#endif
	return true;
}
bool KAcserverManager::endElement(std::string &context, std::string &qName) {
#ifdef ENABLE_MULTI_SERVER
	if (qName == "server" && cur_mserver) {
		KMultiAcserver *mac = conf.gam->refsMultiAcserver(cur_mserver->name);
		if (mac) {
			if (!mac->isChanged(cur_mserver)) {
				cur_mserver->release();
				cur_mserver = mac;
			} else {
				mac->release();
			}
		}		
		if (!addMultiAcserver(cur_mserver)) {
			cur_mserver->release();		
		}
		cur_mserver = NULL;		
	}
#endif
#ifdef ENABLE_VH_RUN_AS
	if (qName == "cmd" && cur_cmd) {		
		KCmdPoolableRedirect *mac = conf.gam->refsCmdRedirect(cur_cmd->name);
		if (mac) {
			if (!mac->isChanged(cur_cmd)) {
				cur_cmd->release();
				cur_cmd = mac;
			} else {
				mac->release();
			}
		}		
		if (!addCmd(cur_cmd)) {
			cur_cmd->release();		
		}
		cur_cmd = NULL;
		cur_extend = NULL;
		return true;
	}
#endif
	if (qName == "api" && cur_extend) {
		cur_extend = NULL;
		return true;
	}
	return true;
}

void KAcserverManager::buildXML(std::stringstream &s, int flag) {
	lock.RLock();
	std::map<std::string,KSingleAcserver *>::iterator it;
	s << "\t<!--server start-->\n";
	for (it = acservers.begin(); it != acservers.end(); it++) {
		if ((*it).second->ext) {
			continue;
		}
		(*it).second->buildXML(s);
	}
#ifdef ENABLE_MULTI_SERVER
	std::map<std::string,KMultiAcserver *>::iterator it2;
	for (it2 = mservers.begin(); it2 != mservers.end(); it2++) {
		if ((*it2).second->ext) {
			continue;
		}
		(*it2).second->buildXML(s);
	}
#endif
	s << "\t<!--server end-->\n";
	s << "\t<!--cgi start-->\n";
	std::map<std::string,KCgiRedirect *>::iterator it3;
	for (it3 = cgis.begin(); it3 != cgis.end(); it3++) {
		if ((*it3).second->ext) {
			continue;
		}
		(*it3).second->buildXML(s);
	}
	s << "\t<!--cgi end-->\n";
	s << "\t<!--api start-->\n";
	std::map<std::string,KApiRedirect *>::iterator it4;
	for (it4 = apis.begin(); it4 != apis.end(); it4++) {
		if ((*it4).second->ext) {
			continue;
		}
		(*it4).second->buildXML(s);
	}
	s << "\t<!--api end-->\n";
#ifdef ENABLE_VH_RUN_AS
	s << "\t<!--cmd start-->\n";
	std::map<std::string,KCmdPoolableRedirect *>::iterator it5;
	for (it5 = cmds.begin(); it5 != cmds.end(); it5++) {
		if ((*it5).second->ext) {
			continue;
		}
		(*it5).second->buildXML(s);
	}
	s << "\t<!--cmd end-->\n";
#endif
	lock.RUnlock();
}
void KAcserverManager::copy(KAcserverManager &a)
{
	lock.WLock();
	apis.swap(a.apis);
	cgis.swap(a.cgis);
	mservers.swap(a.mservers);
	acservers.swap(a.acservers);
#ifdef ENABLE_VH_RUN_AS
	cmds.swap(a.cmds);
#endif
	/*
	std::map<std::string,KMultiAcserver *>::iterator it,it2;
	std::map<std::string,KMultiAcserver *> tmservers = mservers;
	mservers.clear();
	for (it=a.mservers.begin();it!=a.mservers.end();it++) {
		it2 = tmservers.find((*it).first);
		if (it2!=tmservers.end() && !(*it2).second->isChanged((*it).second) ) {
			mservers.insert(std::pair<std::string,KMultiAcserver *>((*it).first,(*it2).second));
			(*it2).second->addRef();
		} else {
			mservers.insert(std::pair<std::string,KMultiAcserver *>((*it).first,(*it).second));
			(*it).second->addRef();
		}
	}
	for (it=tmservers.begin();it!=tmservers.end();it++) {
		(*it).second->release();
	}

	std::map<std::string,KSingleAcserver *>::iterator it3,it4;
	std::map<std::string,KSingleAcserver *> tservers = acservers;
	acservers.clear();
	for (it3=a.acservers.begin();it3!=a.acservers.end();it3++) {
		it4 = tservers.find((*it3).first);
		if (it4!=tservers.end() && !(*it4).second->isChanged((*it3).second) ) {
			acservers.insert(std::pair<std::string,KSingleAcserver *>((*it3).first,(*it4).second));
			(*it4).second->addRef();
		} else {
			acservers.insert(std::pair<std::string,KSingleAcserver *>((*it3).first,(*it3).second));
			(*it3).second->addRef();
		}
	}
	for (it3=tservers.begin();it3!=tservers.end();it3++) {
		(*it3).second->release();
	}

	std::map<std::string,KCmdPoolableRedirect *>::iterator it5,it6;
	std::map<std::string,KCmdPoolableRedirect *> tcmds = cmds;
	cmds.clear();
	for (it5=a.cmds.begin();it5!=a.cmds.end();it5++) {
		it6 = tcmds.find((*it5).first);
		if (it6!=tcmds.end() && !(*it6).second->isChanged((*it5).second) ) {
			cmds.insert(std::pair<std::string,KCmdPoolableRedirect *>((*it5).first,(*it6).second));
			(*it6).second->addRef();
		} else {
			cmds.insert(std::pair<std::string,KCmdPoolableRedirect *>((*it5).first,(*it5).second));
			(*it5).second->addRef();
		}
	}
	for (it5=tcmds.begin();it5!=tcmds.end();it5++) {
		(*it5).second->release();
	}
	*/
	lock.WUnlock();
}
