#ifndef KPROCESS_H
#define KPROCESS_H
#include "global.h"
#include "forwin32.h"
#include "KStream.h"
#ifndef _WIN32

#define ULONG64	              unsigned long long
#endif
/*
�ӽ�����
*/
class KProcess
{
public:
	KProcess();
	~KProcess();
	bool bind(pid_t pid)
	{
		if(this->pid){
			return false;
		}
		this->pid = pid;
		return true;
	}
	pid_t stealPid()
	{
		pid_t val = pid;
#ifndef _WIN32
		pid = 0;
#else
		pid = NULL;
#endif
		return val;
	}
	pid_t getPid()
	{
		return pid;
	}
	time_t getPowerOnTime()
	{
		return lastPoweron;
	}
#ifdef _WIN32
	bool bindProcessId(DWORD id);
#endif
	//�Ѹý��̱��浽�ļ��������������˳�ʱ�ɰ�ȫ����ɱ��
	bool saveFile(const char *dir,const char *unix_file=NULL);

	bool isKilled()
	{
		return killed;
	}
	bool isActive();
	int getProcessId();
	bool kill();
	void detach()
	{
		killed = true;
	}
	int sig;
private:
	void cleanFile();

private:
	pid_t pid;
	bool killed;
	time_t lastPoweron;
	char *file;
};
#endif
