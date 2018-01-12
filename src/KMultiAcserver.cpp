/*
 * KMultiAcserver.cpp
 *
 *  Created on: 2010-6-4
 *      Author: keengo
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
#include <sstream>
#include "KMultiAcserver.h"
#include "global.h"
#include "lang.h"
#include "malloc_debug.h"
#include "KAsyncFetchObject.h"
#include "KAcserverManager.h"
#include "KConfig.h"
using namespace std;
#ifdef ENABLE_MULTI_SERVER
KMultiAcserver::KMultiAcserver() {
	nodes = NULL;
	nodesCount = 0;
	ip_hash = false;
	url_hash = false;
	cookie_stick = false;
	errorTryTime = 20;
	max_error_count = 5;
	
}
KMultiAcserver::~KMultiAcserver() {
	removeAllNode();
	
}
void KMultiAcserver::removeAllNode()
{
	KSockPoolHelper *helper = nodes;
	while (helper) {
		KSockPoolHelper *n = helper->next;
		helper->stopMonitor();
		helper->release();
		nodesCount--;
		if (n==nodes) {
			break;
		}
		helper = n;
	}
	nodes = NULL;
	bnodes.clear();
	vnodes.clear();
	assert(nodesCount==0);
	nodesCount = 0;
}
void KMultiAcserver::enableAllServer()
{
	KSockPoolHelper *helper = nodes;
	while (helper) {
		KSockPoolHelper *n = helper->next;
		helper->enable();
		if (n==nodes) {
			break;
		}
		helper = n;
	}
}
int KMultiAcserver::getCookieStick(const char *attr,const char *cookie_stick_name)
{
	char *buf = strdup(attr);
	if (buf==NULL) {
		return -1;
	}
	int cookie_stick_value = -1;
	char *hot = buf;
	while (*hot) {
		char *p = strchr(hot,';');
		if (p) {
			*p = '\0';
		}
		while (*hot && isspace((unsigned char)*hot)) {
			hot ++;
		}
		char *eq = strchr(hot,'=');
		if (eq) {
			*eq = '\0';
			if (strcmp(hot,cookie_stick_name)==0) {
				cookie_stick_value = atoi(eq+1)-1;
				break;
			}
		}
		if (p==NULL) {
			break;
		}
		hot = p + 1;
	}
	free(buf);
	return cookie_stick_value;
}
unsigned short KMultiAcserver::getNodeIndex(KHttpRequest *rq)
{
	if (cookie_stick) {
		const char *cookie_stick_name = conf.cookie_stick_name;
		if (*cookie_stick_name=='\0') {
			cookie_stick_name = DEFAULT_COOKIE_STICK_NAME;
		}
		KHttpHeader *av = rq->parser.getHeaders();
		while (av) {
			if (strcasecmp(av->attr,"Cookie")==0) {
				int cookie_stick_value = getCookieStick(av->val,cookie_stick_name);
				if (cookie_stick_value>=0) {
					if (cookie_stick_value < (int)vnodes.size()) {
						return (unsigned short)cookie_stick_value;
					}
					break;
				}
			}
			av = av->next;
		}
	}
	unsigned index;
	if (url_hash) {
		index = string_hash(rq->url->host,8,1);
		index = string_hash(rq->url->path,16,index);
	} else if (ip_hash) {
		index = rq->c->socket->addr.get_hash();
	} else {
		index = rand();
	}
	index = index % vnodes.size();
	if (cookie_stick) {
		rq->cookie_stick = index + 1;
	}
	return (unsigned short)index;
}
void KMultiAcserver::connect(KHttpRequest *rq)
{
	KSockPoolHelper *sockHelper = NULL;
	lock.Lock();
	if (!vnodes.empty()) {
		unsigned short index = getNodeIndex(rq);
		sockHelper = vnodes[index];
		if (sockHelper->isEnable()) {
			//the node is active
			sockHelper->addRef();
			sockHelper->hit++;
			lock.Unlock();
			sockHelper->connect(rq);
			sockHelper->release();
			return;
		}
		//look for next active node
		KSockPoolHelper *na = nextActiveNode(sockHelper, index);
		if (na) {
			if (cookie_stick) {
				//����ճ���·�������
				rq->cookie_stick = index + 1;
			}
			na->addRef();
			na->hit++;
			lock.Unlock();
			na->connect(rq);
			na->release();
			return;
		}
	}
	KSockPoolHelper *fast_node = NULL;
	for (size_t i=0;i<bnodes.size();i++) {
		//look for fastest backup node
		KSockPoolHelper *bnode = bnodes[i];
		if (bnode->isEnable()) {
			if (fast_node == NULL || bnode->avg_monitor_tick < fast_node->avg_monitor_tick) {
				fast_node = bnode;
				continue;
			}
		}
	}
	if (fast_node != NULL) {
		fast_node->addRef();
		fast_node->hit++;
		lock.Unlock();
		fast_node->connect(rq);
		fast_node->release();
		return;
	}
	
	//reset all node
	enableAllServer();
	if (sockHelper) {
		sockHelper->addRef();
		sockHelper->hit++;
		lock.Unlock();
		sockHelper->connect(rq);
		sockHelper->release();
	} else {
		lock.Unlock();
		static_cast<KAsyncFetchObject *>(rq->fetchObj)->connectCallBack(rq,NULL);
	}
	return;	
}
KSockPoolHelper *KMultiAcserver::nextActiveNode(KSockPoolHelper *node,unsigned short &index)
{
	KSockPoolHelper *helper = node;
	bool use_next = (index & 1)>0;
	while (helper) {
		KSockPoolHelper *n = (use_next)?helper->next:helper->prev;
		if (use_next) {
			if (index>=vnodes.size()-1) {
				index = 0;
			} else {
				index ++;
			}
		} else {
			if (index==0) {
				index = (unsigned short)vnodes.size() - 1;
			} else {
				index --;
			}
		}
		if (helper->tryTime==0) {
			return helper;
		}
		if (n==node) {
			return NULL;
		}
		helper = n;
	}
	return NULL;
}

unsigned KMultiAcserver::getPoolSize() {
	return 0;
}
bool KMultiAcserver::addNode(std::map<std::string, std::string> &attr)
{
	KSockPoolHelper *sockHelper = new KSockPoolHelper;
	if (!sockHelper->parse(attr)){
		delete sockHelper;
		return false;
	}
	unsigned weight = atoi(attr["weight"].c_str());
	sockHelper->weight = weight;
	sockHelper->setErrorTryTime(max_error_count, errorTryTime);
	lock.Lock();
	addNode(sockHelper);
	buildVNode();
	lock.Unlock();
	return true;
}
bool KMultiAcserver::addNode(std::map<std::string, std::string> &attr, char *self_ip)
{
	char *to_ip = strchr(self_ip, '-');
	if (to_ip) {
		*to_ip = '\0';
		to_ip++;
		sockaddr_i min_addr;
		sockaddr_i max_addr;
		if (!KSocket::getaddr(self_ip, 0, &min_addr, AF_INET, AI_NUMERICHOST)) {			
			return false;
		}
		if (!KSocket::getaddr(to_ip, 0, &max_addr, AF_INET, AI_NUMERICHOST)) {			
			return false;
		}
		uint32_t min_ip = ntohl(min_addr.v4.sin_addr.s_addr);
		uint32_t max_ip = ntohl(max_addr.v4.sin_addr.s_addr);
		for (uint32_t ip = min_ip; ip <= max_ip; ip++) {
			min_addr.v4.sin_addr.s_addr = htonl(ip);
			char ips[MAXIPLEN];
			KSocket::make_ip(&min_addr, ips, MAXIPLEN);
			attr["self_ip"] = ips;
			addNode(attr);
		}
		return true;
	}
	return false;
}
bool KMultiAcserver::editNode(std::map<std::string,std::string> &attr) {
	string action = attr["action"];
	if (action=="edit") {
		int id = atoi(attr["id"].c_str());
		lock.Lock();
		KSockPoolHelper *node = getIndexNode(id);
		if (node) {
			node->weight = atoi(attr["weight"].c_str());
			node->parse(attr);
			buildVNode();
		}
		lock.Unlock();
	} else {
		const char *self_ip = attr["self_ip"].c_str();
		if (self_ip && *self_ip) {
			char *buf = strdup(self_ip);
			if (addNode(attr, buf)) {
				free(buf);
				return true;
			}
			free(buf);
		}
		return addNode(attr);
	}
	return true;
}
void KMultiAcserver::addNode(KSockPoolHelper *sockHelper)
{
	if (nodes==NULL) {
		sockHelper->next = sockHelper;
		sockHelper->prev = sockHelper;
		nodes = sockHelper;
	} else {
		sockHelper->next = nodes;
		sockHelper->prev = nodes->prev;
		nodes->prev->next = sockHelper;
		nodes->prev = sockHelper;
	}
	nodesCount++;
}
std::string KMultiAcserver::nodeForm(std::string name, KMultiAcserver *as,
		unsigned nodeIndex) {

	KSockPoolHelper *helper = NULL;
	if (as) {
		as->lock.Lock();
		helper = as->getIndexNode((int)nodeIndex);
	}
	stringstream s;
	s << "<form action='/macserver_node?name=" << name << "&action=";
	if (helper) {
		s << "edit";
	} else {
		s << "add";
	}
	s << "&id=" << nodeIndex << "' method='post'>\n";
	s << klang["lang_host"] << ": <input name='host' value='";
	if (helper) {
		s << helper->host;
	}
	s  << "'>";
	s << LANG_PORT << ": <input name='port' size='5' value='" << (helper ? helper->port:80) ;

	s << "'>";
	s << "<br>";
	s << klang["lang_life_time"] << ": <input name='life_time' size=6 value='"	<< (helper ? helper->getLifeTime() : 0) << "'><br>";

	s << klang["weight"] << ": <input name='weight' value='"
			<< (helper ? helper->weight : 1) << "'><br>";
	const char *self_ip = NULL;
	if (helper) {
		self_ip = helper->getIp();
	}
	s << "self_ip: <input name='self_ip' value='" << (self_ip ? self_ip : "") << "'><br>";
	s << "<input type=checkbox name='sign' value='1' " << (helper && helper->sign ? "checked":"") << ">sign<br>";
	s << "<input type='submit' value='" << LANG_SUBMIT << "'>";
	s << "</form>";
	if (as) {
		as->lock.Unlock();
	}
	return s.str();
}
KSockPoolHelper *KMultiAcserver::getIndexNode(int index)
{
	if(index>=nodesCount){
		return NULL;
	}
	KSockPoolHelper *helper = nodes;
	while (helper) {
		if (index--<=0) {
			return helper;
		}	
		KSockPoolHelper *n = helper->next;
		helper = n;
	}
	return NULL;
}
void KMultiAcserver::getNodeInfo(std::stringstream &s)
{
	lock.Lock();
	KSockPoolHelper *node = nodes;
	for (int i = 0; i < nodesCount; i++) {
		if (i>0) {
			s << "\n";
		}
		s << node->host;
		if (!node->isUnix) {
			s << ":" << node->port;
		}
		s << "\t" << node->total_connect << "/" << node->hit << "\t" << (node->isEnable()?"OK":"FAILED");
		s << "(" << node->total_error << " " << node->avg_monitor_tick << ")";
		node = node->next;
	}
	lock.Unlock();
}
void KMultiAcserver::getHtml(std::stringstream &s) {
	lock.Lock();
	s << "<tr><td rowspan='" << nodesCount << "'>";
	s << "[<a href=\"javascript:if(confirm('really delete')){ window.location='/del_macserver?name="
			<< name << "';}\">" << LANG_DELETE << "</a>]";
	s << "[<a href='/extends?item=1&name=" << name << "'>";
	s << LANG_EDIT << "</a>]";
	s << "[<a href='/macserver_node_form?name=" << name << "&action=add'>"
			<< klang["add_node"] << "</a>]</td>";
	s << "<td rowspan='" << nodesCount << "'>" << name << "</td>";
	s << "<td rowspan='" << nodesCount << "'>";
	s << KPoolableRedirect::buildProto(proto);
	s << "</td>";
	s << "<td rowspan='" << nodesCount << "'>" ;
	if (url_hash) {
		s << "url";
	} else if (ip_hash) {
		s << "ip";
	} else {
		s << "rand";
	}
	s << "</td>";
	s << "<td rowspan='" << nodesCount << "'>" << (cookie_stick?LANG_ON:"&nbsp;") << "</td>";
	s << "<td rowspan='" << nodesCount << "'>" << errorTryTime << "</td>";
	s << "<td rowspan='" << nodesCount << "'>" << max_error_count << "</td>";
	s << "<td rowspan='" << nodesCount << "'>" << getRefFast()	<< "</td>";
	
	KSockPoolHelper *node = nodes;
	for (int i = 0; i < nodesCount; i++) {
		assert(node);
		if (i > 0) {
			s << "<tr>";
		}
		s
				<< "<td>[<a href=\"javascript:if(confirm('delete this node?')){ window.location='/macserver_node?name="
				<< name << "&id=" << i << "&action=delete';}\">" << LANG_DELETE
				<< "</a>]";
		s << "[<a href='/macserver_node_form?name=" << name << "&id=" << i
				<< "&action=edit'>" << LANG_EDIT << "</a>]"
				<< node->host << "</td>";
		s << "<td>" ;
		if(node->isUnix)
			s << "-";
		else 
			s << node->port ;
		
		s << "</td>";
		s << "<td>" << node->getLifeTime() << "</td>";
		s << "<td>" << node->getSize() << "</td>";
		s << "<td>" << node->weight << "</td>";
		const char *ip = node->getIp();
		s << "<td>" << (ip ? ip : "") << "</td>";
		s << "<td>" << node->total_connect << "/" << node->hit << "</td>";
		s << "<td>" << (node->isEnable() ? "<font color=green>OK</font>" : "<font color=red>FAILED</font>");
		s << "(" << node->total_error << " " << node->avg_monitor_tick << ")</td>";
		s << "</tr>\n";
		node = node->next;
	}
	if (nodesCount == 0) {
		s << "<td colspan=9>&nbsp; </td></tr>";
	}
	lock.Unlock();
}
void KMultiAcserver::baseHtml(KMultiAcserver *mserver,std::stringstream &s)
{
#ifndef HTTP_PROXY
	s << klang["protocol"] << ":";
	s << "<input type='radio' name='proto' value='http' ";
	if (mserver==NULL || mserver->proto==Proto_http) {
		s << "checked";
	}
	s << ">http <input type='radio' name='proto' value='fastcgi' ";
	if(mserver && mserver->proto==Proto_fcgi){
		s << "checked";
	}
	s << ">fastcgi ";
	s << "<input type='radio' value='ajp' name='proto' ";
	if(mserver && mserver->proto==Proto_ajp){
		s << "checked";
	}
	s << ">ajp";
	s << "<input type='radio' value='uwsgi' name='proto' ";
	if(mserver && mserver->proto==Proto_uwsgi){
		s << "checked";
	}
	s << ">uwsgi";
	s << "<input type='radio' value='scgi' name='proto' ";
		if(mserver && mserver->proto==Proto_scgi){
		s << "checked";
	}
	s << ">scgi";
	s << "<input type='radio' value='hmux' name='proto' ";
	if(mserver && mserver->proto==Proto_hmux){
		s << "checked";
	}
	s << ">hmux";
	
	s << "<br>";
#endif
	s << "<input type='checkbox' name='ip_hash' value='1' ";
	if (mserver && mserver->ip_hash) {
		s << "checked";
	}
	s << ">" << klang["ip_hash"] ;
	s << "<input type='checkbox' name='url_hash' value='1' ";
	if (mserver && mserver->url_hash) {
		s << "checked";
	}
	s << ">url_hash";
	s << "<input type='checkbox' name='cookie_stick' value='1' ";
	if (mserver && mserver->cookie_stick) {
		s << "checked";
	}
	s << ">" << klang["cookie_stick"] << "<br>";
	s << klang["error_try_time"] << "<input name='error_try_time' value='" << (mserver?mserver->errorTryTime:30) << "' size='4'><br>";
	s << klang["error_count"] << "<input name='max_error_count' value='" << (mserver?mserver->max_error_count:5) << "' size='4'><br>";
	
}
std::string KMultiAcserver::form(KMultiAcserver *mserver)
{
	std::stringstream s;
	s << "<form action='/macserveradd?action=" << (mserver?"edit":"add") << "' method=post>\n";
	s << LANG_NAME << ":<input name=name value='";
	if (mserver) {
		s << mserver->name;
	}
	s << "' ";
	if (mserver) {
		s << "readonly";
	}
	s << "><br>";
	baseHtml(mserver,s);
	s << "<input type=submit value=" << LANG_ADD << "></form>\n";
	return s.str();
}
void KMultiAcserver::parseNode(const char *nodeString)
{
	char *buf = strdup(nodeString);
	lock.Lock();
	removeAllNode();
	char *hot = buf;
	while (*hot) {
		char *p = strchr(hot,',');
		if (p) {
			*p = '\0';
		}
		char *port = NULL;
		if (*hot=='[') {
			hot ++;
			port = strchr(hot,']');
			if (port) {
				*port = '\0';
				port++;
				port = strchr(port,':');
			}
		} else {
			port = strchr(hot,':');
		}
		if (port) {
			*port = '\0';
			port ++;
			KSockPoolHelper *sockHelper = new KSockPoolHelper;
			sockHelper->setHostPort(hot, port);
			char *lifeTime = strchr(port,':');
			if (lifeTime) {
				*lifeTime='\0';
				lifeTime++;
				sockHelper->setLifeTime(atoi(lifeTime));
				char *weight = strchr(lifeTime,':');
				if (weight) {
					*weight = '\0';
					weight ++;
					sockHelper->weight = atoi(weight);
					char *ip = strchr(weight, ':');
					if (ip) {
						*ip = '\0';
						ip++;
						sockHelper->setIp(ip);
					}
				}
			}
			addNode(sockHelper);
		}
		if (p==NULL) {
			break;
		}
		hot = p+1;
	}
	buildVNode();
	lock.Unlock();
	free(buf);
}
void KMultiAcserver::setErrorTryTime(int max_error_count,int errorTryTime)
{
	if (errorTryTime > 600) {
		errorTryTime = 600;
	}
	if (errorTryTime < 5) {
		errorTryTime = 5;
	}
	lock.Lock();
	this->max_error_count = max_error_count;
	this->errorTryTime = errorTryTime;
	KSockPoolHelper *helper = nodes;
	while (helper) {
		KSockPoolHelper *n = helper->next;
		n->setErrorTryTime(max_error_count, errorTryTime);
		helper->error_try_time = errorTryTime;
		helper->max_error_count = max_error_count;
		if (n == nodes) {
			break;
		}
		helper = n;
	}
	lock.Unlock();
}

void KMultiAcserver::parse(std::map<std::string,std::string> &attribute)
{
	
	lock.Lock();
	if (name.size()==0) {
		name = attribute["name"];
	}
	url_hash = attribute["url_hash"]=="1";
	if (!url_hash) {
		ip_hash = attribute["ip_hash"]=="1";
	} else {
		ip_hash = false;
	}
	cookie_stick = attribute["cookie_stick"]=="1";
	proto = KPoolableRedirect::parseProto(attribute["proto"].c_str());
	lock.Unlock();
	setErrorTryTime(atoi(attribute["max_error_count"].c_str()), atoi(attribute["error_try_time"].c_str()));
}
bool KMultiAcserver::delNode(int nodeIndex) {
	bool result = false;
	lock.Lock();
	KSockPoolHelper *helper = nodes;
	while (helper) {
		if (nodeIndex--<=0) {
			break;
		}	
		KSockPoolHelper *n = helper->next;
		helper = n;
	}
	if (helper) {
		nodesCount--;
		if (helper==nodes) {
			nodes=nodes->next;
		}
		if (helper==helper->next) {
			assert(nodesCount==0);
			nodes = NULL;
		} else {
			helper->prev->next = helper->next;
			helper->next->prev = helper->prev;
		}
		helper->stopMonitor();
		helper->release();
		buildVNode();
	}
	lock.Unlock();
	return result;
}
void KMultiAcserver::buildVNode()
{
	vnodes.clear();
	bnodes.clear();
	KSockPoolHelper *helper = nodes;
	while (helper) {
		KSockPoolHelper *n = helper->next;
		if (helper->weight==0) {
			bnodes.push_back(helper);
		} else {
			for (int i=0;i<helper->weight;i++) {
				vnodes.push_back(helper);
			}
		}
		if (n==nodes) {
			break;
		}
		helper = n;
	}
}
void KMultiAcserver::buildAttribute(std::stringstream &s)
{
	s << "proto='";
	s << KPoolableRedirect::buildProto(proto);
	s << "' ip_hash='" << (ip_hash?1:0) << "' ";
	s << "url_hash='" << (url_hash?1:0) << "' ";
	s << "cookie_stick='" << (cookie_stick?1:0) << "' ";
	s << "error_try_time='" << errorTryTime << "' ";
	s << "max_error_count='" << max_error_count << "' ";
	
}
void KMultiAcserver::buildXML(std::stringstream &s) {
	s << "\t<server name='" << name << "' ";
	buildAttribute(s);
	s << ">\n";
	lock.Lock();
	KSockPoolHelper *helper = nodes;
	while (helper) {
		KSockPoolHelper *n = helper->next;
		s << "\t\t<node  weight='" << helper->weight << "'";
		helper->buildXML(s);
		s << "/>\n";
		if (n==nodes) {
			break;
		}
		helper = n;
	}
	lock.Unlock();
	s << "\t</server>\n";
}
bool KMultiAcserver::isChanged(KPoolableRedirect *rd)
{
	if (KPoolableRedirect::isChanged(rd)) {
		return true;
	}
	KMultiAcserver *ma = static_cast<KMultiAcserver *>(rd);
	if (this->nodesCount!=ma->nodesCount) {
		return true;
	}
	if (ma->ip_hash != this->ip_hash) {
		return true;
	}
	if (ma->url_hash!=this->url_hash) {
		return true;
	}
	if (ma->cookie_stick != this->cookie_stick) {
		return true;
	}
	if (ma->errorTryTime != this->errorTryTime) {
		return true;
	}
	if (ma->max_error_count != this->max_error_count) {
		return true;
	}
	KSockPoolHelper *helper = nodes;
	KSockPoolHelper *maHelper = ma->nodes;
	while (helper) {
		assert(maHelper);
		if (helper->isChanged(maHelper)) {
			return true;
		}
		helper = helper->next;
		maHelper = maHelper->next;
		if (helper==nodes) {
			break;
		}
	}
	
	return false;
}
#endif
