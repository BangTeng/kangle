/*
 * KCmdPoolableRedirect.cpp
 *
 *  Created on: 2010-8-26
 *      Author: keengo
 */
#include "utils.h"
#include "KCmdPoolableRedirect.h"
#include "KApiRedirect.h"
#include "KCmdProcess.h"
#include "http.h"

using namespace std;
#ifdef ENABLE_VH_RUN_AS
KCmdPoolableRedirect::KCmdPoolableRedirect() {
	//lifeTime = EXTENDPROGRAM_DEFAULT_LIFETIME;
	lockGlobal = false;
	type = WORK_TYPE_SP;
	chuser = true; 
#ifndef _WIN32
	sig = SIGKILL;
#endif
	port = 0;
}
KCmdPoolableRedirect::~KCmdPoolableRedirect() {

}
void KCmdPoolableRedirect::connect(KHttpRequest *rq)
{
/*	if (pm == NULL) {
		klog(KLOG_ERR, "no init the process manage\n");
		handleError(rq,STATUS_SERVER_ERROR,"no init the process manage");
		return;
	}
*/
	pm.connect(rq, this);
}
void KCmdPoolableRedirect::buildXML(std::stringstream &s) {
	s << "\t<cmd name='" << name << "' proto='";
	s << KPoolableRedirect::buildProto(proto) << "'";
	s << " file='" << cmd << "'";
	//s << " split_char='" << split_char << "'";
	s << " type='" << getTypeString(type) << "'";
	if(!chuser){
		s << " chuser='0'";
	}
	if(lockGlobal){
		s << " lock='1'";
	}
	if(worker>0){
		s << " worker='" << worker << "'";
	}
	if(port>0){
		s << " port='" << port << "'";
	}
#ifndef _WIN32
	if (sig>0 && sig!=SIGKILL) {
		s << " sig='" << sig << "'";
	}
#endif
	KExtendProgram::buildConfig(s);
	if(envs.size()>0){
		s << "\t\t<env ";
		for(std::map<std::string,std::string>::iterator it = envs.begin();it!=envs.end();it++){
			s << (*it).first << "=\"" << (*it).second << "\" ";
		}
		s << "/>\n";
	}
	s << "\t</cmd>\n";
}

KUpstreamSelectable *KCmdPoolableRedirect::createPipeStream(KVirtualHost *vh,KListenPipeStream *st,std::string &unix_path,bool isSameRunning){
	vector<char *> args;
	int rdst = RDSTD_NONE;
	KExtendProgramString ds(name.c_str(),vh);
	Token_t token = NULL;
	bool result = false;
	KUpstreamSelectable *socket = NULL;
	st->process.sig = sig;
#ifdef KSOCKET_UNIX	
	bool unix_socket = conf.unix_socket;
#endif
	if(chuser){
		token = vh->createToken(result);
		if (!result) {
			return NULL;
		}
	}
	char *cmd_buf = xstrdup(cmd.c_str());
	explode_cmd(cmd_buf,args);
	int args_count = args.size() + 1;
	bool saveProcessToFile = true;
	if (type==WORK_TYPE_MP && worker>0) {
		args_count++ ;
		rdst = RDSTD_NAME_PIPE;
		st->process.detach();	
		saveProcessToFile = false;
	}
	char **arg = new char *[args_count];
	size_t i = 0;
	if(type==WORK_TYPE_MP){
		if(worker>0){
			arg[i] = strdup(conf.extworker.c_str());
			i++;
		} else {
			rdst = RDSTD_INPUT;
		}
	}
	for (unsigned j=0; j < args.size(); j++) {
		char *a = ds.parseString(args[j]);
		if(a==NULL){
			continue;
		}
		if(*a=='\0'){
			free(a);
			continue;
		}
		arg[i] = a;
		i++;
	}
	arg[i] = NULL;
	bool locked = lockGlobal;
	if(locked){
		lockCommand();
	}
	int port2 = 0;
	
	if (isSameRunning || preLoad(&ds)) {
		KCmdEnv *env = makeEnv(&ds);
		result = ::createProcess(st, token, arg, env, rdst);
		if (env) {
			delete env;
		}
		if(result) {
			if (saveProcessToFile) {
				st->process.saveFile(conf.tmppath.c_str(),(unix_path.size()>0?unix_path.c_str():NULL));
			}
			ds.setPid(st->process.getProcessId());
			if (type==WORK_TYPE_MP) {
				if (worker>0) {
					FCGI_Header h;
					memset(&h,0,sizeof(FCGI_Header));
					h.type = CMD_CREATE_PROCESS;
	#ifdef KSOCKET_UNIX	
					if(unix_socket){
						h.type = CMD_CREATE_PROCESS_UNIX;
					}
	#endif
					h.id = worker;
					st->write_all((char *)&h,sizeof(FCGI_Header));
					if (st->read_all((char *)&h,sizeof(FCGI_Header))) {
						st->setPort(h.id);
					}
	#ifdef KSOCKET_UNIX	
					if(unix_socket){
						std::stringstream s;
						s << "/tmp/extworker." << h.id << ".sock";
						s.str().swap(unix_path);
					}
	#endif
				}
			} else {
				
				port2 = port;
			}	
			if (port2==0) {
				port2 = st->getPort();
			} else {
				st->setPort(port);
			}

			//st->portMap.swap(ds.port);
			st->closeServer();
			if (port2 > 0) {	
				//debug("cmd port=%d\n",port);
				//第一次连接，要点时间(10秒)
				for (int i = 0; i < (int)conf.time_out; i++) {
					socket = new KUpstreamSelectable(new KClientSocket);
					if (unix_path.size()>0) {
#ifdef KSOCKET_UNIX	
						if (socket->socket->connect(unix_path.c_str(),1)) {
							debug("connect to unix socket [%s] success\n",unix_path.c_str());
							break;
						}
#endif
					} else {
						if (socket->socket->connect("127.0.0.1", port2, 1)) {
							//socket->set_time(60);
							debug("connect to port %d success\n",port2);
							break;
						}
					}
					socket->destroy();
					socket = NULL;
					if (!st->process.isActive()) {
						break;
					}
					debug("cann't connect to port %d try it again (%d/%d)\n",port2,i,conf.time_out);
					sleep(1);
				}
			}
			postLoad(&ds);
		}
	}
	if (locked) {
		unlockCommand();
	}
	for (i=0;;i++) {
		if(arg[i]==NULL){
			break;
		}
		xfree(arg[i]);
	}
	delete[] arg;
	xfree(cmd_buf);
	if (token) {
		KVirtualHost::closeToken(token);
	}
#ifndef _WIN32
	if(socket){
		socket->socket->setnoblock();
	}
#endif
	return socket;
}
bool KCmdPoolableRedirect::setWorkType(const char *typeStr,bool changed) {
	int type = getTypeValue(typeStr);
	if (type != WORK_TYPE_MP) {
		type = WORK_TYPE_SP;
		pm.setWorker(1);
	} else {	
		if(worker<0){
			worker = 0;
		}
		pm.setWorker(worker);
	}
	if (this->type != type) {
		this->type = type;
		changed = true;
	}
	if (changed) {
		pm.clean();
	}
	std::stringstream s;
	s << "cmd:" << name;
	pm.setName(s.str().c_str());
	return true;
}
bool KCmdPoolableRedirect::parseEnv(std::map<std::string, std::string> &attribute)
{
	if (KExtendProgram::parseEnv(attribute)) {
		pm.clean();
		return true;
	}
	return false;
}
void KCmdPoolableRedirect::parseConfig(std::map<std::string, std::string> &attribute)
{
	KExtendProgram::parseConfig(attribute);
	bool changed = false;
	Proto_t proto = KPoolableRedirect::parseProto(attribute["proto"].c_str());
	if (this->proto!=proto) {
		this->proto = proto;
		changed = true;
	}
	if (cmd!=attribute["file"]) {
		changed = true;
		cmd = attribute["file"];
	}
	int worker = atoi(attribute["worker"].c_str());
	if (this->worker != worker) {
		changed = true;
		this->worker = worker;
	}
	bool chuser = (attribute["chuser"] != "0");
	if (this->chuser != chuser) {
		changed = true;
		this->chuser = chuser;
	}
	int port = atoi(attribute["port"].c_str());
	if (this->port != port) {
		changed = true;
		this->port = port;
	}
	int sig = atoi(attribute["sig"].c_str());
#ifndef _WIN32
	if(sig==0){
		sig = SIGKILL;
	}
#endif
	if (this->sig != sig) {
		changed = true;
		this->sig = sig;
	}
	if(attribute["lock"] == "1"||attribute["lock"]=="on"){
		lockGlobal = true;
	}else{
		lockGlobal = false;
	}
	setWorkType(attribute["type"].c_str(),changed);
}
bool KCmdPoolableRedirect::isChanged(KPoolableRedirect *rd)
{
	if (KPoolableRedirect::isChanged(rd)) {
		return true;
	}
	KCmdPoolableRedirect *c = static_cast<KCmdPoolableRedirect *>(rd);
	if (cmd!=c->cmd) {
		return true;
	}
	if (worker!=c->worker) {
		return true;
	}
	if (chuser!=c->chuser) {
		return true;
	}
	if (port!=c->port) {
		return true;
	}
	if (sig!=c->sig) {
		return true;
	}
	if (lockGlobal!=c->lockGlobal) {
		return true;
	}
	if (type!=c->type) {
		return true;
	}
	return KExtendProgram::isChanged(c);
}
#endif

