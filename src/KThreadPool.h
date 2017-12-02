#ifndef ThreadPool_H_skdfjsldfjaldskfjlsdkfj
#define ThreadPool_H_skdfjsldfjaldskfjlsdkfj
#ifndef _WIN32
#include<pthread.h>
#endif

#include<list>
#include<map>

#include "utils.h"
#include "KMutex.h"
#include "forwin32.h"

#define COMMAND_THREAD_NULL		0
#define COMMAND_THREAD_START	1
#define COMMAND_THREAD_END		2
#define NOTICE_THREAD_SIG		SIGUSR2

typedef FUNC_TYPE FUNC_CALL (*ThreadFunc)(void *);
struct ThreadInfo
{
	void *param;
	int cmd;
	ThreadFunc work;
	pthread_t pid;
	unsigned runCount;
	time_t end_time;

};

typedef std::list<ThreadInfo *> ThreadInfoList;
class KThreadPool
{
public:
	KThreadPool();
	~KThreadPool();
	void Flush(unsigned min_free_thread = 4);
	bool start(void *param,ThreadFunc work,bool usePool=true);
	int getFreeThread();
	void closeAllFreeThread();
	void setStackSize(unsigned size);
	int get_work_thread_count();
private:
#ifndef _WIN32
	pthread_attr_t attr;
#endif
	pthread_t id;
};
extern KThreadPool m_thread;
extern int total_thread;
void set_stack_size(std::string stack_size);
int set_max_per_ip(int value);
#endif
