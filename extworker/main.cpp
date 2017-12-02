#include <stdarg.h>
#include <map>
#include <stdio.h>
#include "KSocket.h"
#include "api_child.h"
#include "extworker.h"
#include "KListenPipeStream.h"
#include "KChildListen.h"
int argc;
char **argv;
int m_debug = 0;
extern std::map<pid_t,time_t> processes;
extern KMutex processLock;
extern std::map<u_short, KApiDso *> apis;
volatile bool program_quit = false;
extern KListenPipeStream ls;
void debug(const char *fmt, ...) {
#ifndef NDEBUG
	//if (m_debug) {
		va_list ap;
		va_start(ap,fmt);
		vprintf(fmt, ap);
		va_end(ap);
	//}
#endif
}
void klog(int level, const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
}
void killallProcess()
{
#ifndef _WIN32
	signal(SIGCHLD,SIG_IGN);
#endif
	program_quit = true;
	debug("now kill all processes program_quit=%d\n",program_quit);
	processLock.Lock();
	std::map<pid_t,time_t>::iterator it;
	for(it=processes.begin();it!=processes.end();it++){
#ifdef _WIN32
		TerminateProcess((*it).first,0);
#else
		kill((*it).first,SIGKILL);
#endif
	}
	processLock.Unlock();
	std::map<u_short, KApiDso *>::iterator it2;
	for(it2=apis.begin();it2!=apis.end();it2++){
		(*it2).second->unload();
	}
	ls.unlink_unix();
	if (cl) {
		cl->unlink_unix();
	}
}
#ifndef _WIN32
bool childExsit()
{
	int status;
	int ret,child;
	int rc;
	ret = waitpid(-1,&status,WNOHANG);
	child = ret;
	switch (ret) {
        case 0:
                return false;
        case -1:
                return false;
        default:
		printf("child %d exsit\n",child);
                if (WIFEXITED(status)) {
                        fprintf(stderr, "extworker: child exited with: %d\n", WEXITSTATUS(
                                        status));
                        rc = WEXITSTATUS(status);
                } else if (WIFSIGNALED(status)) {
                        fprintf(stderr, "extworker: child signaled: %d\n",
                                        WTERMSIG(status));
                        rc = 1;
                } else {
                        fprintf(stderr, "extworker: child died somehow: exit status = %d\n",
                                        status);
                        rc = status;
                }
		if (!program_quit) {
			restart_child_process(child);
		}
        }
        return true;


}
void sigcatch(int sig) {
	switch(sig)
	{
		case SIGCHLD:
			while(childExsit());
			break;
		default:
			program_quit = true;
			killallProcess();
			exit(0);
	}
}
#endif
void seperate_usage();
int main(int argc,char **argv)
{
	::argc = argc;
	::argv = argv;
	KSocket::init_socket();

#ifndef _WIN32
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, sigcatch);
	signal(SIGTERM, sigcatch);
	signal(SIGUSR1, sigcatch);
	signal(SIGUSR2, sigcatch);
	signal(SIGQUIT, sigcatch);
#endif
	if (argc==1 || (argc>1 && strcmp(argv[1],"-h")==0)) {
		seperate_usage();
		return 1;
	}
	if (argc>4) {
		if (strcmp(argv[1],"-b")==0) {
			//��������ģʽ
			seperate_work_model();
			return 0;
		}
	}
	KPipeStream st;

	signal(SIGCHLD, sigcatch);
	st.fd[0] = 4;
	st.fd[1] = 5;

	for (;;) {
		if (!api_child_process(&st)) {
			break;
		}
	}
	killallProcess();
	_exit(0);
	return 0;
}
