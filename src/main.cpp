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

#include "global.h"
#include "forwin32.h"
#ifndef _WIN32
#include <pthread.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "forwin32.h"
#else
#include <direct.h>
#include <stdlib.h>
#include "forwin32.h"
#include <Mswsock.h>
#define _WIN32_SERVICE
#endif
#include<iostream>
#include <time.h>
#include <string>
#include <stdio.h>
#ifdef ENABLE_JEMALLOC
#include <jemalloc/jemalloc.h>
#endif
#include "utils.h" 
#include "do_config.h"
#include "log.h"
#include "server.h"
#include "extern.h"
//#define OPEN_FILE
#include "http.h"
#include "cache.h"
#include "KListenConfigParser.h"
#include "KThreadPool.h"
#include "KAcserverManager.h"
#include "KWriteBackManager.h"
#include "KSelectorManager.h"
#include "malloc_debug.h"
#include "KXml.h"
#include "KVirtualHostManage.h"
#include "KHttpServerParser.h"
#include "KFastcgiFetchObject.h"
#include "KSingleProgram.h"
#include "KServerListen.h"
#include "KProcessManage.h"
#include "KGzip.h"
#include "KLogElement.h"
#include "KSSIProcess.h"
#include "api_child.h"
#include "time_utils.h"
#include "directory.h"
#include "KHttpDigestAuth.h"
#include "KObjectList.h"
#include "KAcserverManager.h"
#include "KVirtualHostDatabase.h"
#include "KCdnContainer.h"
#include "KWriteBackManager.h"
#include "KDynamicListen.h"
#include "KAddr.h"
#include "KMemPool.h"
#include "KLogDrill.h"


#ifndef HAVE_DAEMON
int daemon(int nochdir, int noclose);
#endif
char *lang = NULL;
int m_debug = 0;
bool skipCheckRunning = false;
time_t program_start_time = time(NULL);
int program_rand_value ;
using namespace std;
int m_pid = 0;
int m_ppid = 0;
extern int my_uid ;
int child_pid = 0;
int reboot_flag = 0;
int serial = 0 ;
int worker_index = 0;
bool nodaemon = false;
bool nofork = false;
int open_file_limit = 0;
/*
 * 定义是否以一个cmd扩展运行
 */
bool cmd_extend = false;
bool test();
int GetNumberOfProcessors();
extern int numberCpu;
#ifdef _WIN32
HANDLE active_event = INVALID_HANDLE_VALUE;
HANDLE shutdown_event = INVALID_HANDLE_VALUE;
HANDLE signal_pipe = INVALID_HANDLE_VALUE;
static HANDLE notice_event = INVALID_HANDLE_VALUE;
void start_safe_service();
std::vector<WorkerProcess *> workerProcess;
#else
std::map<int,WorkerProcess *> workerProcess;
#endif
/*
 the main process and child process communicate pipe
 */
const char *serverType = "free";
void my_exit(int code)
{
#ifdef _WIN32
	SetUnhandledExceptionFilter(NULL);
#endif
	conf.gam->unloadAllApi();
#ifdef _WIN32
	TerminateProcess(GetCurrentProcess(),code);
#endif
	exit(code);
}
#ifndef _WIN32
void killworker(int sig)
{
	std::map<int,WorkerProcess *>::iterator it;
	for (it=workerProcess.begin();it!=workerProcess.end();it++) {
		kill((*it).first,sig);
	}
	/*
	if (sig==SIGHUP) {
		for(it=workerProcess.begin();it!=workerProcess.end();it++){
			delete (*it).second;
		}
		workerProcess.clear();
	}
	*/
}
#endif
//}}
#ifdef MALLOCDEBUG
extern "C" {
	extern void __libc_freeres();
}
#ifdef _WIN32
void LogEvent(LPCTSTR pFormat, ...);
#endif
#endif
void checkMemoryLeak()
{	
#ifdef MALLOCDEBUG
	if (!conf.mallocdebug) {
		fprintf(stderr,"mallocdebug is not active\n");
		return;
	}
	my_msleep(1000);
	printf("free all know memory\n");
	selectorManager.close();
#ifndef HTTP_PROXY
	conf.gam->killAllProcess();
#endif
#ifdef ENABLE_VH_FLOW
	conf.gvm->dumpFlow();
#endif
	int i;
	for (i = 0; i < 2; i++) {
		kaccess[i].destroy();
	}
	for (size_t j=0;j<conf.service.size();j++) {
		delete conf.service[j];
	}
	conf.service.clear();
	delete conf.sysHost;
	conf.sysHost = NULL;
	
#ifndef HTTP_PROXY
	delete conf.gvm;
	delete conf.gam;
#endif
#ifdef ENABLE_WRITE_BACK
	writeBackManager.destroy();
#endif
	cache.freeAllObject();
#ifdef ENABLE_DIGEST_AUTH
	KHttpDigestAuth::flushSession(kgl_current_sec + 172800);
#endif
	cdnContainer.flush(kgl_current_sec + 172800);
	klang.clear();
	conf.admin_ips.clear();
	if (conf.dnsWorker) {
		conf.dnsWorker->release();
		conf.dnsWorker = NULL;
	}
	if (conf.ioWorker) {
		conf.ioWorker->release();
		conf.ioWorker = NULL;
	}
	printf("wait for all thread close\n");
	for (;;) {
		if (m_thread.get_work_thread_count() == 0) {
			break;
		}
		my_msleep(100);
	}
	selectorManager.destroy();
	m_thread.closeAllFreeThread();
	my_msleep(1500);
	dump_memory(0, -1);	
#ifndef _WIN32
	__libc_freeres();
#endif
	exit(0);
#endif
}
void shutdown() {
	if (quit_program_flag != PROGRAM_QUIT_IMMEDIATE) {
		return;
	}
	cache.shutdown_disk(true);
	dlisten.close();
#ifdef MALLOCDEBUG
	dlisten.clear();
#endif
	conf.default_cache = 0;
#ifdef ENABLE_DISK_CACHE
	if (conf.disk_cache > 0) {
		saveCacheIndex();
	}
#endif	
#ifdef ENABLE_VH_RUN_AS
	conf.gam->killAllProcess();
#endif
#ifdef ENABLE_VH_FLOW
	conf.gvm->dumpFlow();
#endif
	klog(KLOG_INFO, "shutdown now\n");
	accessLogger.close();
	errorLogger.close();
	singleProgram.deletePid();
	quit_program_flag = PROGRAM_QUIT_SHUTDOWN;
#ifndef MALLOCDEBUG
	my_exit(0);
#endif
}
void shutdown_signal(int sig)
{
#ifndef _WIN32
	if (workerProcess.size() > 0) {
		/*
		* 如果是主进程则立即退出。
		*/
		killworker(sig);
		if (sig!=SIGHUP) {
			quit_program_flag = PROGRAM_QUIT_IMMEDIATE;
		}
		return;
	}
#endif
	if (quit_program_flag) {
	    klog(KLOG_DEBUG, "have another thread set quit flags\n");
	    return;
	}
	quit_program_flag = PROGRAM_QUIT_IMMEDIATE;
}

#ifdef ENABLE_DISK_CACHE
bool create_dir(const char *dir) {
	mkdir(dir,448);
	return true;
}
void create_cache_dir(const char *disk_dir) {
	string path;
	if(disk_dir && *disk_dir){
		path = disk_dir;
		pathEnd(path);
	} else {
		path  = conf.path;
		path += "cache";
		path += PATH_SPLIT_CHAR;
	}
	KStringBuf s;
	create_dir(path.c_str());
	for (int i = 0; i <= CACHE_DIR_MASK1; i++) {
		s << path.c_str();
		s.addHex(i);
		if(!create_dir(s.getString())){
			return;
		}
		s.clean();
		for (int j = 0; j <= CACHE_DIR_MASK2; j++) {
			s << path.c_str();
			s.addHex(i);
			s << PATH_SPLIT_CHAR;
			s.addHex(j);
			if(!create_dir(s.getString())){
				return;
			}
			s.clean();
		}
	}
	KStringBuf index_name;
	index_name << path << "index";
	FILE *fp = fopen(index_name.getString(), "wb");
	if (fp == NULL) {
		fprintf(stderr, "cann't open cache index file for write[%s]\n", index_name.getString());
		return;
	}
	HttpObjectIndexHeader indexHeader;
	memset(&indexHeader, 0, sizeof(HttpObjectIndexHeader));
	indexHeader.head_size = sizeof(HttpObjectIndexHeader);
	indexHeader.block_size = sizeof(HttpObjectIndex);
	indexHeader.state = INDEX_STATE_CLEAN;
	indexHeader.cache_dir_mask1 = CACHE_DIR_MASK1;
	indexHeader.cache_dir_mask2 = CACHE_DIR_MASK2;
	fwrite((char *) &indexHeader, 1, sizeof(indexHeader), fp);
	fclose(fp);
	fprintf(stderr, "create cache dir success\n");
}
#endif
void console_call_reboot() {
	quit_program_flag = PROGRAM_QUIT_IMMEDIATE;
	shutdown();

}

void sigcatch(int sig) {
#ifdef HAVE_SYSLOG_H
	klog(KLOG_INFO,"catch signal %d,my_pid=%d\n", sig, getpid());
#endif
#ifndef _WIN32
	//int status = 0;
	//int ret;
	signal(sig, sigcatch);
	switch (sig) {
	case SIGTERM:
	case SIGINT:
	case SIGQUIT:
		shutdown_signal(sig);
		break;
	case SIGHUP:
		if(workerProcess.size()>0){
			killworker(sig);
		} else {
			configReload = true;
		}
		break;
	case SIGUSR2:
		if(workerProcess.size()>0){
			killworker(SIGUSR2);
		} else {
#ifdef ENABLE_VH_FLOW
			flushFlowFlag = true;
#endif
#ifdef MALLOCDEBUG
			dump_memory_object = true;
#endif
		}
		break;
	default:
		return;
	}
#endif
}
void set_user() {
#if	!defined(_WIN32)
	if(conf.run_user.size()>0){
		int uid,gid;
		if (getuid() != 0) {
			fprintf(stderr, "I am not root user,cann't run as user[%s]\n", conf.run_user.c_str());
			return;
		}
		bool result = name2uid(conf.run_user.c_str(),uid,gid);
		if (!result) {
			klog(KLOG_ERR,"cann't find run_as user [%s]\n",conf.run_user.c_str());
		}
		if (result && conf.run_group.size()>0) {
			result = name2gid(conf.run_group.c_str(),gid);
			if (!result) {
				klog(KLOG_ERR,"cann't find run_as group [%s]\n",conf.run_group.c_str());
			}
		}
		if (result) {
			chown(conf.tmppath.c_str(),uid,gid);
			setgid(gid);
			setuid(uid);
		}
		
	}
	
#endif	/* !_WIN32 */
}
void list_service() {
	return;
}
int service_to_signal(int sig, bool showError = true) {
	if (m_pid == 0) {
		if (showError) {
			fprintf(stderr, "Error,program is not running.\n");
		}
		return 0;
	}
	if (kill(m_pid, sig) == 0) {
		return m_pid;
	}
	if (showError) {
		fprintf(stderr, "Error ,while kill signal to pid=%d.\n", m_pid);
	}
	return 0;
}

bool create_file_path(char *argv0) {
	if (!get_path(argv0, conf.path)) {
		return false;
	}
	KFileName file;
	file.tripDir(conf.path);
#ifndef _WIN32
	conf.path = "/" + conf.path;
#endif
	conf.program = conf.path + PATH_SPLIT_CHAR + conf.program;
	conf.extworker = conf.path + PATH_SPLIT_CHAR + "extworker";
#ifdef _WIN32
	conf.extworker += ".exe";
#endif
	int p = conf.path.find_last_of(PATH_SPLIT_CHAR);
	if (p > 0) {
		conf.path = conf.path.substr(0, p + 1);
	}
#ifdef KANGLE_TMP_DIR
	conf.tmppath = KANGLE_TMP_DIR;
	conf.tmppath += PATH_SPLIT_CHAR;
#else
	conf.tmppath = conf.path + PATH_SPLIT_CHAR + "tmp" + PATH_SPLIT_CHAR;
#endif
	mkdir(conf.tmppath.c_str(),448);
	return true;
}
void shutdown_process(int pid,int sig)
{
}
int clean_process_handle(const char *file,void *param)
{
	int kangle_pid = *((int *)(param));
	if(filencmp(file,"kp_",3)!=0){
		return 0;
	}
	int fpid = atoi(file+3);
	if (kangle_pid>0 && fpid!=kangle_pid) {
		return 0;
	}
	int pid = 0;
	int sig = 0;
	const char *p = strchr(file+3,'_');
	if (p) {
		pid = atoi(p+1);
		p = strchr(p+1,'_');
		if(p){
			sig = atoi(p+1);
		}
#ifdef _WIN32
		HANDLE hProcess = OpenProcess(PROCESS_TERMINATE,FALSE,pid);
		if(hProcess!=NULL){
			TerminateProcess(hProcess,sig);
			CloseHandle(hProcess);
		}
#else
		kill(pid,sig);
#endif
	}
	std::stringstream s;
	s << conf.tmppath << file;
	char unix_file[512];
	FILE *fp = fopen(s.str().c_str(),"rb");
	if(fp){
		int len = fread(unix_file,1,sizeof(unix_file)-1,fp);
		if(len>0){
			unix_file[len] = '\0';
			unlink(unix_file);
		}
		fclose(fp);
	}
	unlink(s.str().c_str());
	return 0;
}
void clean_process(int pid)
{
	list_dir(conf.tmppath.c_str(),clean_process_handle,(void *)&pid);
}

static int Usage(bool only_version = false) {
	printf(PROGRAM_NAME "/" VERSION "(%s) build with support:"
#ifdef KSOCKET_IPV6
			" ipv6"
#endif
#ifdef KSOCKET_SSL
			" ssl["
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
			"S"
#endif
#ifdef TLSEXT_TYPE_next_proto_neg
			"N"
#endif
#ifdef TLSEXT_TYPE_application_layer_protocol_negotiation
			"A"
#endif
#ifdef ENABLE_KSSL_BIO
			"B"
#endif
			"] "
#endif
#ifdef _LARGE_FILE
			" large-file"
#endif
#ifdef ENABLE_HTTP2
			" http2"
#endif
		
#ifdef ENABLE_DISK_CACHE
			" disk-cache"
#endif
#ifdef ENABLE_SQLITE_DISK_INDEX
			" sqlite-disk-index"
#endif
#ifdef MALLOCDEBUG
			" malloc-debug"
#endif
#ifdef ENABLE_KSAPI_FILTER
			" ksapi-filter"
#endif
#ifndef NDEBUG
		" debug"
#endif
			"\n", getServerType());
	printf("pcre version: %s\n",pcre_version());
#ifdef KSOCKET_SSL
	printf("openssl version: %s\n",SSLeay_version(SSLEAY_VERSION));
#endif
#ifdef UPDATE_CODE
	printf("UPDATE_CODE: %s\n",UPDATE_CODE);
#endif
	if (!only_version) {
		printf("Usage: " PROGRAM_NAME " [-hlqnra:] [-d level]\n"
		"   (no param to start server.)\n"
		"   [-h --help]      print the current message\n"
		"   [-d level]       start in debug model,level=0-3\n"
		"   [-r --reload]    reload config file graceful\n"
#ifndef _WIN32
		"   [--reboot]       reboot server\n"
#endif
#ifdef ENABLE_DISK_CACHE
		"   [-z [disk_dir]]  create disk cache directory\n"
#endif
		"   [-v --version]   show program version\n"
//		"   [-a apache_config_file]  convert apache config\n"
#ifndef _WIN32
		"   [-q]             shutdown\n"
		"   [-n]             start program not in daemon\n"
#endif
		"Report bugs to <keengo99@gmail.com>.\n"
		"");
	}
#ifdef ENABLE_JEMALLOC
	const char *j;
        size_t s = sizeof(j);
	mallctl("version", &j,  &s, NULL, 0);
	printf("jemalloc version: [%s]\n",j);
#endif
	//checkMemoryLeak();
	my_exit(0);
	return 0;
}
bool create_path(char **argv) {
	char *argv0 = NULL;
#ifdef _WIN32
	char szFilename[512];
	::GetModuleFileName(NULL, szFilename, sizeof(szFilename)-1);
	argv0=xstrdup(szFilename);
	conf.diskName = szFilename;
	conf.diskName = conf.diskName.substr(0,2);
#else
	argv0 = xstrdup(argv[0]);
#endif
	if (!create_file_path(argv0)) {
		xfree(argv0);
		return false;
	}
	xfree(argv0);
	return true;
}
int parse_args(int argc, char ** argv) {
	extern char *optarg;
	int ret = 0;
#ifdef _WIN32
	char tmp[512];
#endif
	conf.log_level = -1;
	string pidFile;
#ifdef KANGLE_VAR_DIR
        pidFile = KANGLE_VAR_DIR;
#else
        pidFile = conf.path;
	pidFile += "/var";
#endif
	mkdir(pidFile.c_str(),0700);
	pidFile += PID_FILE;
	m_pid = singleProgram.checkRunning(pidFile.c_str());
	/*
	if (singleProgram.checkRunning(pidFile.c_str())) {
		m_pid = singleProgram.pid;
		if (m_pid <= 0) {
			fprintf(
					stderr,
					"Something error,have another program is running,but the pid=%d is not right.\n",
					m_pid);
			my_exit(1);
		}
	}
	*/
	if (argc > 1) {
		ret = 1;
	}
#ifndef _WIN32
	int c;
	struct option long_options[] = { { "reload", 0, 0, 'r' },
	{ "version", 0, 0, 'v' }, 
	{ "help", 0, 0,	'h' }, 
	{ "reboot", 0, 0, 'b' }, 
	{ 0, 0, 0, 0 } };
	int opt_index = 0;
	while ((c = getopt_long(argc, argv, "lgnrz:mfqa:cd:hvr?", long_options,
			&opt_index)) != -1) {
		switch (c) {
		case 0:
			break;
		case 'b':
			ret = 0;
			service_to_signal(SIGINT, false);
			skipCheckRunning = true;
			break;

		case 'q':
			m_pid = service_to_signal(SIGTERM);
			if (m_pid>0) {
				for (int i=0;i<200;i++) {
					if (kill(m_pid,0)!=0) {
						break;
					}
					my_msleep(200);
				}
				printf("shutdown success.\n");
				my_exit(0);
			}
			printf("shutdown error.\n");
			my_exit(1);
			break;
#ifdef ENABLE_VH_FLOW
			case 'f':
			service_to_signal(SIGUSR2);
			my_exit(0);
			break;
#endif
		case 'r':
			service_to_signal(SIGHUP);
			my_exit(0);
			break;
#ifdef MALLOCDEBUG
		case 'm':
			service_to_signal(SIGUSR2);
			my_exit(0);
			break;
#endif
		case 'n':
			ret = 0;
			nodaemon = true;
			break;
		case 'g':
			nofork = true;
			break;
		case 'c':
			ret = 0;
			cmd_extend = true;
			nofork = true;
			nodaemon = true;
			break;
		case 'd':
			ret = 0;
			m_debug = atoi(optarg);
			nodaemon = true;
			printf("run as debug model(level=%d).\n", m_debug);
			break;
		case 'v':
			Usage(true);
			my_exit(0);
#ifdef ENABLE_DISK_CACHE
		case 'z':
			create_cache_dir(optarg);
			my_exit(0);
#endif
		case 'a':
			my_exit(merge_apache_config(optarg));
		case 'h':
		case '?':
			Usage();
			my_exit(0);
		default:
			if (!cmd_extend) {
				Usage();
				my_exit(0);
			}
		}
	}
#else

#endif
	if ((ret == 0) && (m_pid != 0) && !skipCheckRunning) {
		fprintf(stderr, "Start error,another program (pid=%d) is running.\n",
				m_pid);
		fprintf(stderr, "Try (%s -q) to close it.\n", argv[0]);
		my_exit(1);
	}
	return ret;
}
void init_signal() {
#ifndef _WIN32
	umask(0022);
	signal(SIGPIPE, SIG_IGN);
	/*
	 * SIGHUP   用来reload config
	 */
	signal(SIGHUP, sigcatch);
	/*
	 * SIGINT SIGTERM 用来退出程序.
	 */
	signal(SIGINT, sigcatch);
	signal(SIGTERM, sigcatch);
	/*
	 * SIGUSR1  重新加载vh.xml
	 */
	signal(SIGUSR1, sigcatch);
	/*
	 * SIGUSR2 用来flush flux,用来dump memory
	 */
	signal(SIGUSR2, sigcatch);
	/*
	 * SIGQUIT  用来reboot的。
	 */
	signal(SIGQUIT, sigcatch);
	signal(SIGCHLD, SIG_DFL);
#endif
}
//初始化安全进程
void init_safe_process()
{
#ifdef KANGLE_ETC_DIR
	string configFile = KANGLE_ETC_DIR;
#else
	string configFile = conf.path + "/etc";
#endif
	configFile += CONFIG_FILE;
	listenConfigParser.parse(configFile.c_str());
}
void init_stderr()
{
#ifdef ENABLE_TCMALLOC
	close(2);
	string stderr_file = conf.path + "/var/stderr.log";
	KFile fp;
	if (fp.open(stderr_file.c_str(),fileAppend)) {
		FILE_HANDLE fd = fp.stealHandle();
		dup2(fd,2);
		fprintf(stderr,"stderr is open success\n");
	}
#endif
}
bool init_resource_limit(int numcpu)
{
	bool result = true;
#ifndef _WIN32
	//adjust max open file
	struct rlimit rlim;
	unsigned open_file_limited = 65536 * numcpu;
	if (open_file_limited < 65536) {
		open_file_limited = 65536;
	}
	if (0==getrlimit(RLIMIT_NOFILE,&rlim)) {
		if (rlim.rlim_max < open_file_limited) {
			rlim.rlim_cur = open_file_limited;
			rlim.rlim_max = open_file_limited;
			int ret = setrlimit(RLIMIT_NOFILE,&rlim);
			if (ret!=0) {
				klog(KLOG_ERR,"set open file limit error [%d]\n",errno);
				result = false;
			}
		}
	}
	if (0==getrlimit(RLIMIT_NOFILE,&rlim)) {
		klog(KLOG_ERR,"max open file limit [cur:%d,max:%d]\n",rlim.rlim_cur,rlim.rlim_max);
		open_file_limit = rlim.rlim_max;
	} else {
		klog(KLOG_ERR,"get max open file limit error [%d]\n",errno);
	}
#endif
	return result;
}
#ifdef _WIN32
unsigned getpagesize() {
	SYSTEM_INFO  si;
	GetSystemInfo(&si);
	return si.dwPageSize;
}
#endif
void init_program() {
	//printf("sizeof (rq) = %d\n",sizeof(KHttpRequest));
#ifdef ENABLE_LOG_DRILL
	init_log_drill();
#endif
	init_aio_align_size();
	spProcessManage.setName("api:sp");
	initFastcgiData();
	kgl_pagesize = getpagesize();
	init_addr_worker();
	int select_count = conf.select_count;
	if(select_count<=0){
		select_count = numberCpu;
		if (select_count==0) {
			select_count = 1;
		}
	}
	
	selectorManager.init(select_count);
}

#ifndef _WIN32
int create_worker_process(int index)
{
	worker_index = index;
	int pid = fork();
	if (pid==0) {
		//child
		//singleProgram.unlock();
		std::map<int,WorkerProcess *>::iterator it;
		for(it = workerProcess.begin();it!=workerProcess.end();it++){
			delete (*it).second;
		}
		workerProcess.clear();
		for (size_t i = 0; i < conf.service.size(); i++) {
			delete conf.service[i];
		}
		conf.service.clear();
	}
	return pid;
}
#endif
void my_fork() {
#ifndef _WIN32
	init_safe_process();
	std::map<int,WorkerProcess *>::iterator it;
	for (;;) {
		if (workerProcess.size()==0) {
			if (quit_program_flag>0) {
				m_pid = 0;
				singleProgram.deletePid();
				my_exit(0);
				break;
		
			}
			for (size_t i=0;i<1;i++) {
				int pid = create_worker_process(i);
				if (pid==0) {
					return;
				}
				if (pid<0) {
					continue;
				}
				WorkerProcess *process = new WorkerProcess;
				process->pid = pid;
				process->worker_index = i;
				workerProcess.insert(pair<int,WorkerProcess *>(pid,process));
			}
		}
		int status;
		int pid = waitpid(-1,&status,WNOHANG);
		if (pid<=0) {
			sleep(1);
			continue;
		}	
		it = workerProcess.find(pid);
		if (it==workerProcess.end()) {
			continue;
		}
		WorkerProcess *process = (*it).second;
		clean_process(process->pid);
		if (WEXITSTATUS(status)==100) {
			shutdown();
		}
		workerProcess.erase(it);
		if (quit_program_flag == PROGRAM_NO_QUIT) {
			pid = create_worker_process(process->worker_index);
			if (pid==0) {
				return;
			}
			if (pid<0) {
				fprintf(stderr,"create worker process failed,errno=%d\n",errno);
				continue;
			}
			process->pid = pid;
			workerProcess.insert(pair<int,WorkerProcess *>(pid,process));
		} else {
			delete process;
		}
	}
#endif
}
void StartAll() {
	init_signal();
#ifndef _WIN32
	if (!nodaemon && m_debug == 0) {
		daemon(0, 0);
	}
	save_pid();
	if (!nofork) {
		my_fork();
		singleProgram.unlock();
	}
	signal(SIGCHLD, SIG_IGN);
#endif

#ifdef _WIN32
	if (worker_index==0) {
		create_signal_pipe();
	}
	if (kflike(shutdown_event) || kflike(signal_pipe)) {
		m_thread.start(NULL,signal_thread);
	}
	if (worker_index==0) {
		save_pid();
	}
#endif
	updateTime();
#ifdef KSOCKET_SSL
	init_ssl();
#endif
	do_config(true);
	klog_start();
	m_pid = getpid();
	parse_config(true);
	init_program();
	if (cmd_extend) {
		//forcmdextend();
		fprintf(stderr,"don't support cmd extend model\n");
	} else {
		if (worker_index == 0) {
			conf.gvm->flush_static_listens(conf.service);
		}
	}
	if (dlisten.listens.empty()) {
		klog(KLOG_ERR, "No any listen , program start failed\n");
		my_exit(100);
	}
	for (int i = numberCpu; i > 0; i--) {
		if (init_resource_limit(i)) {
			break;
		}
	}
	set_user();
#ifndef _WIN32
	my_uid = getuid();
	m_ppid = getppid();
#endif
	program_start_time = time(NULL);
	klog(KLOG_NOTICE, "Start success [pid=%d].\n",m_pid);

	conf.gvm->addAllVirtualHost();
#ifdef ENABLE_VH_RUN_AS	
	conf.gam->loadAllApi();
#endif
#ifdef ENABLE_TF_EXCHANGE
	if (worker_index==0) {
		m_thread.start(NULL,clean_tempfile_thread);
	}
#endif
	selectorManager.start();
	time_thread(NULL);
#ifdef MALLOCDEBUG
	checkMemoryLeak();
#endif
	KSocket::clean_socket();
}
void StopAll() {
	shutdown_signal(0);
}

int main(int argc, char **argv) {

	srand((unsigned) time(NULL));
	program_rand_value = rand();
	KSocket::init_socket();
	KSSIProcess::init();

	if (!create_path(argv)) {
		fprintf(stderr,
				"cann't create path,don't start kangle in search path\n");
#ifdef _WIN32
		LogEvent("cann't create path\n");
#endif
		my_exit(0);
	}
	LoadDefaultConfig();

	numberCpu = GetNumberOfProcessors();
	//printf("number of cpus %d\n",numberCpu);
	if(numberCpu<=0){
		numberCpu = 1;
	}
	//	printf("using LANG %s\n",lang);
	if (parse_args(argc, argv)) {
		Usage();
		my_exit(0);
	}

	StartAll();
	return 0;
}

void save_pid() {
	std::string path;
#ifdef KANGLE_VAR_DIR
	path = KANGLE_VAR_DIR;
#else
	path = conf.path;
	path += "/var";
#endif
	path += PID_FILE;
	singleProgram.lock(path.c_str());
}
