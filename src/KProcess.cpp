#ifndef _WIN32
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <signal.h>
#endif
#include <sstream>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "KProcess.h"
#include "do_config.h"
#include "log.h"
#include "utils.h"
#include "malloc_debug.h"

int numberCpu = 1;
#ifdef SOLARIS
int GetNumberOfProcessors()
{
	return 2;
}
#endif
#ifdef _WIN32
typedef void (WINAPI *PGNSI)(LPSYSTEM_INFO);
int GetNumberOfProcessors()
{
	SYSTEM_INFO si;
	// Call GetNativeSystemInfo if supported or GetSystemInfo otherwise.
	PGNSI pfnGNSI = (PGNSI) GetProcAddress(GetModuleHandle("kernel32.dll"), "GetNativeSystemInfo");
	if(pfnGNSI)
	{
		pfnGNSI(&si);
	}
	else
	{
		GetSystemInfo(&si);
	}
	return si.dwNumberOfProcessors;
}

#endif
#ifdef BSD_OS
#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysctl.h>
int GetNumberOfProcessors()
{
	int ncpus = 1;
	int mib[2];
	size_t len = sizeof(ncpus);
	mib[0] = CTL_HW;
	mib[1] = HW_NCPU;
	if (sysctl(mib, 2, &ncpus, &len, NULL, 0) == 0) {
         	return ncpus;
	}
	return 1;
}

#endif
#ifdef LINUX
int GetNumberOfProcessors()
{
	int ncpus = -1;
#if defined(_SC_NPROCESSORS_ONLN)
	/* Linux, Solaris, Tru64, UnixWare 7, and Open UNIX 8  */
	ncpus = sysconf(_SC_NPROCESSORS_ONLN);
#elif defined(_SC_NPROC_ONLN)
	/* IRIX */
	ncpus = sysconf(_SC_NPROC_ONLN);
#else
#warning "Please port this function"
#endif

	if (ncpus == -1) {
		return 1;
	}
	if(ncpus==0){
		return 1;
	}
	return ncpus;
}

#endif

KProcess::KProcess() {
#ifndef _WIN32
	pid = 0;	
	sig = SIGTERM;
#else
	pid = NULL;
	sig = 0;
#endif

	killed = false;
	file = NULL;
	lastPoweron = time(NULL);
}
KProcess::~KProcess() {
	kill();
#ifdef _WIN32
	if (pid) {
		CloseHandle(pid);
	}
#endif
}
void KProcess::cleanFile()
{
	if (file) {
		unlink(file);
		free(file);
		file = NULL;
	}
}
bool KProcess::saveFile(const char *dir,const char *unix_file)
{
	std::stringstream s;
	s << dir << "kp_" << getpid() << "_" << getProcessId() << "_" << sig;
	kassert(file==NULL);
	file = strdup(s.str().c_str());
	KFile fp;
	if (fp.open(file,fileWrite)) {
		if (unix_file) {
			fp.fprintf("%s",unix_file);
		}
		return true;
	} else {
		free(file);
		file = NULL;
		return false;
	}
}

int KProcess::getProcessId() {
#ifdef _WIN32
	return GetProcessId(pid);
#else
	return pid;
#endif
}
bool KProcess::kill() {
	cleanFile();
	if (killed) {
		return true;
	}
	killed = true;
	bool result = true;
#ifndef _WIN32
	if (pid > 0) {
		if (sig == 0) {
			sig = SIGTERM;
		}
		debug("now kill %d to Child pid=%d\n", sig,pid);
		result = (::kill(pid, sig)==0);
		pid = 0;
	}
#else
	if (pid) {
		result = (TerminateProcess(pid,sig)==TRUE);
		CloseHandle(pid);
		pid = NULL;
	}
#endif
	return result;
}
#ifdef _WIN32
bool KProcess::bindProcessId(DWORD id)
{
	return false;
}
#endif
bool KProcess::isActive()
{
#ifndef _WIN32
	if(pid==0){
		return false;
	}
	bool result = (0 == ::kill(pid,0));
#else
	if(pid==NULL){
		return false;
	}
	bool result = (WAIT_TIMEOUT == WaitForSingleObject(pid,0));
#endif
	if(!result){
		klog(KLOG_ERR,"process pid=[%d] is crashed\n",getProcessId());
	}
	return result;
}
