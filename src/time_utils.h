#ifndef TIMEUTILS_H
#define TIMEUTILS_H
#include "KMutex.h"
#include "extern.h"
#include "kserver.h"
#include "KConfig.h"
#ifndef _WIN32
#include <sys/time.h>
#endif
const char *mk1123time(time_t time, char *buf, int size);
int make_http_time(time_t time,char *buf,int size);
const char *log_request_time(char *tstr,size_t buf_size);
extern volatile char cachedDateTime[sizeof("Mon, 28 Sep 1970 06:00:00 GMT")+2];
extern volatile char cachedLogTime[sizeof("[28/Sep/1970:12:00:00 +0600]")+2];
extern volatile bool configReload;

extern KMutex timeLock;

inline void setActive()
{
	
}
void updateTime();
#endif