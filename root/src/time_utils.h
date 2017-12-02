#ifndef TIMEUTILS_H
#define TIMEUTILS_H
#include "KMutex.h"
#include "extern.h"
#include "server.h"
#include "KConfig.h"
#ifndef _WIN32
#include <sys/time.h>
#endif
const char * mk1123time(time_t time, char *buf, int size);
int make_http_time(time_t time,char *buf,int size);
const char *log_request_time(char *tstr,size_t buf_size);
void adjustTime(INT64 diffTime);
//#include "lib.h"
extern volatile char cachedDateTime[sizeof("Mon, 28 Sep 1970 06:00:00 GMT")+2];
extern volatile char cachedLogTime[sizeof("[28/Sep/1970:12:00:00 +0600]")+2];
extern volatile INT64 kgl_current_msec;
extern volatile time_t kgl_current_sec; 

extern KMutex timeLock;

inline void setActive()
{
		
}
inline void updateTime()
{
	struct timeval   tv;
	gettimeofday(&tv,NULL);
	if (unlikely(kgl_current_sec!=tv.tv_sec)) {
		if (tv.tv_sec < kgl_current_sec) {
			//printf("发生时间倒退\n");
			INT64 diff_msec = (INT64) tv.tv_sec * 1000 + (tv.tv_usec/1000) - kgl_current_msec;
			adjustTime(diff_msec);
		}
		timeLock.Lock();
		kgl_current_sec = tv.tv_sec;
		mk1123time(kgl_current_sec, (char *)cachedDateTime, sizeof(cachedDateTime));
		log_request_time((char *)cachedLogTime,sizeof(cachedLogTime));
		timeLock.Unlock();
		setActive();
		if (unlikely(configReload)) {
			do_config(false);
			configReload = false;
		}
		if (unlikely(quit_program_flag>0)) {
			shutdown();
		}
	}
	kgl_current_msec = (INT64) tv.tv_sec * 1000 + (tv.tv_usec/1000);
	return;
}
#endif

