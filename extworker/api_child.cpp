/*
 * api_child.cpp
 * kangle ���ӽ�������ʱ��������ͨ�ŵĴ���
 *
 *  Created on: 2010-7-9
 *      Author: keengo
 */
#include <stdlib.h>
#include <vector>
#include <map>
#include <sstream>
//#include "KApiRedirect.h"
#include "KStream.h"
#include "KFastcgiUtils.h"
#include "KApiFetchObject.h"
#include "KChildListen.h"
//#include "KSelectorManager.h"

#include "KThreadPool.h"

#include "api_child.h"
#include "KChildApiService.h"
#include "KApiDso.h"
#include "api_child.h"
#include "KListenPipeStream.h"
#include "extworker.h"
std::map<pid_t,time_t> processes;
std::map<u_short, KApiDso *> apis;
int api_child_key;
KListenPipeStream ls;
#ifdef _WIN32
HANDLE api_child_token = NULL;
#endif
KMutex processLock;
using namespace std;
KChildListen *cl = NULL;
u_short cur_api_id = 1;
static KMutex kgl_api_lock;
void restart_child_process(pid_t pid)
{
	bool process_is_too_short = false;
	processLock.Lock();
	std::map<pid_t,time_t>::iterator it = processes.find(pid);
	if (it!=processes.end()) {
		if (time(NULL) - (*it).second < 5) {
			process_is_too_short = true;
		}
		processes.erase(it);
	}
	processLock.Unlock();
#ifdef _WIN32
	CloseHandle(pid);
#endif
	if (process_is_too_short) {
		sleep(1);
	}
	if(program_quit){
		return;
	}
	if(!::createProcess(&ls, NULL, argv, NULL, RDSTD_INPUT)){
                debug("cann't create process\n");
                return ;
        }
        pid = ls.process.stealPid();
	processLock.Lock();
        processes.insert(pair<pid_t,time_t>(pid,time(NULL)));
	processLock.Unlock();
	return;	
}

FUNC_TYPE FUNC_CALL api_listen_thread(void *param) {
	KChildListen *cl = (KChildListen *) param;
	for (;;) {
		cl->canRead();
	}
	KTHREAD_RETURN;
}
static KApiDso *getApiRedirect(u_short id) {
	KApiDso *rd = NULL;
	std::map<u_short, KApiDso *>::iterator it;
	kgl_api_lock.Lock();
	it = apis.find(id);
	if (it != apis.end()) {
		rd = (*it).second;
		//rd->addRef();
	}
	kgl_api_lock.Unlock();
	return rd;
}
static bool loadApiRedirect(const char *path,u_short id) {
	
	KApiDso *rd = new KApiDso;
	rd->path = path;
	if (!rd->load()) {
		delete rd;
		return false;
	}
	kgl_api_lock.Lock();
	apis[id] = rd;
	kgl_api_lock.Unlock();
	return true;
}
static bool api_child_load(KStream *st, char *msg,u_short id, u_short content_len) {

	FCGI_Header header;
	memset(&header, 0, sizeof(header));
	if(loadApiRedirect(msg,id)){
		header.id = 0;
	} else {
		header.id = 1;
	}
	header.type = API_CHILD_LOAD_RESULT;
	return st->write_all((char *) &header, sizeof(header))
			== STREAM_WRITE_SUCCESS;
}
#ifndef _WIN32
static bool api_child_setuid(KPoolableStream *st, char *msg,
		u_short content_len) {
	if (content_len != sizeof(api_child_t_uidgid)) {
		return false;
	}
	FCGI_Header header;
	memset(&header, 0, sizeof(header));
	api_child_t_uidgid *body = (api_child_t_uidgid *) msg;
	//	bool result = true;
	if (body->gid>0 && setgid(body->gid) != 0) {
		header.id |= 2;
	}
	if (body->uid>0 && setuid(body->uid) != 0) {
		header.id |= 1;
	}
	header.type = API_CHILD_SETUID_RESULT;
	if (st->write_all((char *) &header, sizeof(header)) != STREAM_WRITE_SUCCESS) {
		return false;
	}
	return header.id == 0;
}
static bool api_child_chroot(KPoolableStream *st, char *msg,
		u_short content_len) {
	FCGI_Header header;
	memset(&header, 0, sizeof(header));
	header.id = chroot(msg);
	header.type = API_CHILD_CHROOT_RESULT;
	return st->write_all((char *) &header, sizeof(header))
			== STREAM_WRITE_SUCCESS;
}

#endif
static bool api_child_shutdown(KStream *st) {

	return true; 
}
bool api_child_begin_request(KClientSocket *st, char *msg,
		u_short content_len) {
	debug("begin_request\n");
	st->set_delay();
	FCGI_BeginRequestBody *body = (FCGI_BeginRequestBody *) msg;
	if (content_len != sizeof(FCGI_BeginRequestBody)) {
		debug("recv wrong data\n");
		return false;
	}

	if (body->id == 0) {
		debug("body id = %d is wrong\n",body->id);
		//todo:default fs fetchobject
		return false;
	}
	KApiDso *rd = getApiRedirect(body->id);
	if (rd == NULL) {
		debug("cann't get dso id=%d\n",body->id);
		return false;
	}
	KFastcgiStream<KClientSocket> client;
	client.extend = true;
	client.setStream(st);
	KChildApiService *fo = new KChildApiService(rd);
	//releaseApiRedirect(rd);
	if (fo == NULL) {
		debug("cann't make KChildApiService\n");
		return false;
	}
	//fo->st = &client;
	if (!fo->start(&client)) {
		debug("sendHead failed\n");
		delete fo;
		return false;
	}
	st->set_nodelay();
	delete fo;
	return true;
}
void seperate_usage()
{
	printf("usage: extworker -b <[ip]:port|unix:file> childs args ...\n");
}
void seperate_work_model()
{
	char *listen = argv[2];
	bool result;
	int port;
#ifdef KSOCKET_UNIX
	if (strncasecmp(listen,"unix:",5)==0) {
		port = 0;
		listen += 5;
		result = ls.listen(listen);
		if(!result){
			fprintf(stderr,"cann't listen unix socket [%s]\n",listen);
			exit(1);
		}
		printf("success listen to unix socket [%s]\n",listen);
	} else  {
#endif
		if(*listen==':'){
			port = atoi(listen+1);
			result = ls.listen(port,NULL);
		} else {
			char *p = strchr(listen,':');
			if (p==NULL) {
				seperate_usage();
				exit(1);
			}
			*p = '\0';
			port = atoi(p+1);
			result = ls.listen(port,listen);
		}
		if(!result){
			fprintf(stderr,"cann't listen to port [%d]\n",port);
			exit(1);
		}
		printf("success listen to port [%d]\n",port);
#ifdef KSOCKET_UNIX
	}
#endif
	int childs = atoi(argv[3]);
	if (childs<=0) {
		seperate_usage();
		exit(1);
	}
	int total_successed = 0;
	argv+=4;
	for(int i=0;i<childs;i++){
		if(!::createProcess(&ls, NULL, argv, NULL, RDSTD_INPUT)){
			fprintf(stderr,"cann't create process\n");
			break;
		}
		total_successed ++;
		pid_t pid = ls.process.stealPid();
		printf("succss create child pid=%d\n",pid);
		processes.insert(pair<pid_t,bool>(pid,true));
	}
	if (total_successed==0) {
		return;
	}
#ifdef _WIN32
	//sleep(1);
	watch_process(NULL);
#else
	void childExsit();
	for (;;) {
		childExsit();
		sleep(1);
	}
#endif
}
bool cmd_create_process(KPoolableStream *st,FCGI_Header *header,bool unix_socket)
{

	FCGI_Header rh;
	memset(&rh,0,sizeof(FCGI_Header));
	bool result = false;
	if (header->id==0 || argc<=1) {
		goto done;
	}
#ifdef KSOCKET_UNIX	
	if(unix_socket){
		std::stringstream s;
		rh.id = getpid();
		s << "/tmp/extworker." << rh.id << ".sock";
		if (!ls.listen(s.str().c_str())) {
			goto done;
		}
	} else {
#endif
		if (!ls.listen()) {
			goto done;
		}
		rh.id = ls.getPort();
#ifdef KSOCKET_UNIX
	}
#endif
	argv++;
	for(int i=0;i<header->id;i++){
		if(!::createProcess(&ls, NULL, argv, NULL, RDSTD_INPUT)){
			goto done;
		}
		pid_t pid = ls.process.stealPid();
		processes.insert(pair<pid_t,bool>(pid,true));
	}
	result = true;
#ifdef _WIN32
	sleep(1);
	m_thread.start(NULL,watch_process);
#endif
done:
	rh.type = CMD_CREATE_PROCESS_RESULT;
	st->write_all((char *)&rh,sizeof(FCGI_Header));
	return result;
}
bool api_child_request(KClientSocket *st) {
	FCGI_Header header;
	bool result = true;
	char *msg = NULL;
	if (!st->read_all((char *) &header, sizeof(header))) {
		fprintf(stderr, "cann't read from parent process,errno=%d\n",errno);
		return false;
	}
	unsigned short content_len = ntohs(header.contentLength);
	if (content_len > 0) {
		msg = (char *) xmalloc(content_len);
		if (msg == NULL) {
			return false;
		}
		if (!st->read_all(msg, content_len)) {
			debug("cann't read data from parent\n");
			xfree(msg);
			return false;
		}
	}
	debug("recv a cmd=[%d]\n", header.type);
	switch (header.type) {
	case FCGI_STDIN:
		//the stdin end
		if (header.contentLength != 0) {
			debug("**********wrong pipe have data still\n");
		}
		break;
	case FCGI_BEGIN_REQUEST:
		result = api_child_begin_request(st, msg, content_len);
		break;
	default:
		debug("recv a unknow cmd[%d]\n", header.type);
		result = false;
	}
	if (msg) {
		xfree(msg);
	}
	return result;
}
bool api_child_process(KPoolableStream *st) {
	FCGI_Header header;
	bool result = true;
	char *msg = NULL;
	if (!st->read_all((char *) &header, sizeof(header))) {
#ifndef _WIN32
		if(errno==EINTR){
			return true;
		}
#endif
		fprintf(stderr, "read from parent process errno=%d\n",errno);
		return false;
	}
	unsigned short content_len = ntohs(header.contentLength);
	if (content_len > 0) {
		msg = (char *) xmalloc(content_len);
		if (msg == NULL) {
			return false;
		}
		if (!st->read_all(msg, content_len)) {
			debug("cann't read data from parent\n");
			xfree(msg);
			return false;
		}
	}
	debug("recv a cmd=[%d]\n", header.type);
	switch (header.type) {
	case FCGI_STDIN:
		//the stdin end
		if (header.contentLength != 0) {
			debug("**********wrong pipe have data still\n");
		}
		break;
	case API_CHILD_LISTEN:
		result = api_child_listen(header.id, static_cast<KPipeStream *> (st),false);
		break;
	case API_CHILD_LISTEN_UNIX:
		result = api_child_listen(header.id, static_cast<KPipeStream *> (st),true);
		break;
	case API_CHILD_LOAD:
		result = api_child_load(st, msg, header.id,content_len);
		break;
#ifndef _WIN32
	case API_CHILD_CHROOT:
		result = api_child_chroot(st, msg, content_len);
		break;
	case API_CHILD_SETUID:
		result = api_child_setuid(st, msg, content_len);
		break;
		
#endif
	case API_CHILD_SHUTDOWN:
		api_child_shutdown(st);
		result = false;
		break;
	case CMD_CREATE_PROCESS:
		result = cmd_create_process(st,&header,false);
		break;
	case CMD_CREATE_PROCESS_UNIX:
		result = cmd_create_process(st,&header,true);
		break;
	default:
		debug("recv a unknow cmd[%d]\n", header.type);
		result = false;
	}
	if (msg) {
		xfree(msg);
	}
	return result;
}
bool api_child_listen(u_short port, KPipeStream *st,bool unix_socket) {
	//printf("enter api_child_listen\n");
	assert(cl==NULL);
	cl = new KChildListen;
	//conf.keep_alive = -1;
	cl->st = st;
	//cl->rd = rd;
	sp_info pi;
	api_child_key = rand();
	pi.key = api_child_key;
	pi.pid = getpid();
	pi.result = 0;
#ifdef KSOCKET_UNIX	
	if (unix_socket) {
		KUnixServerSocket *server = new KUnixServerSocket;
		std::stringstream s;
		pi.port = getpid();
		s << "/tmp/extworker." << pi.port << ".sock";
		s.str().swap(cl->unix_path);
		if(!server->open(cl->unix_path.c_str())){
			pi.result = 1;
		}
		cl->server = server;
	} else {
#endif
		cl->server = new KServerSocket;
		if (!cl->server->open(0, "127.0.0.1")) {
			if (!cl->server->open(0, "::1")) {
				if (!cl->server->open(0)) {
					pi.result = 1;
				}
			}
		}
		if (pi.result == 0) {
			pi.port = cl->server->get_self_port();
		}
#ifdef KSOCKET_UNIX	
	}
#endif
	FCGI_Header header;
	memset(&header, 0, sizeof(header));
	header.type = API_CHILD_LISTEN_RESULT;
	header.contentLength = htons(sizeof(sp_info));
	st->write_all((char *) &header, sizeof(header));
	debug("child listen result=%d\n", pi.result);
	if (st->write_all((char *) &pi, sizeof(pi)) != STREAM_WRITE_SUCCESS) {
		delete cl;
		return false;
	}
	if (pi.result != 0) {
		return false;
	}

	//KChildPipeStreamListen *pipeListen = new KChildPipeStreamListen;
	//pipeListen->st = st;
	return m_thread.start(cl,api_listen_thread,false);
	//for (;;) {
	//	cl->canRead();
	//}
}
FUNC_TYPE FUNC_CALL api_child_thread(void *param) {
	KClientSocket *client = (KClientSocket *) param;
	for(;;){
		if(!api_child_request(client)){
			break;
		}
	}
	delete client;
	KTHREAD_RETURN;
}
