#include "KLogDrill.h"
#ifdef ENABLE_LOG_DRILL
#include "KMutex.h"
#include "KConfig.h"
#include "KFile.h"
#include "KHttpRequest.h"
#include "KVirtualHost.h"

static KMutex drill_lock;
static int drill_count = 0;
static int total_count = 0;
static int drill_hit = 0;
static klog_drill drill_list;
void init_log_drill()
{
	klist_init(&drill_list);
}
void free_log_drill(klog_drill *ld)
{
	xfree(ld->buf);
	delete ld;
}
void add_log_drill(KHttpRequest *rq,KStringBuf &s)
{
	drill_lock.Lock();
	total_count++;
	if (!TEST(rq->filter_flags, RF_LOG_DRILL)) {
		drill_lock.Unlock();
		return;
	}
	drill_hit++;
	if (drill_count >= conf.log_drill) {
		klog_drill *tmp = drill_list.next->next;
		if (tmp != &drill_list) {
			klist_remove(tmp);
			free_log_drill(tmp);
			drill_count--;
		}
	}
	s.WSTR("#[");
	if (rq->svh && rq->svh->vh) {
		s.write_all(rq->svh->vh->name.c_str(), rq->svh->vh->name.size());
	}
	s.WSTR("]\n");
	drill_count++;
	klog_drill *ld = new klog_drill;
	ld->len = s.getSize();
	ld->buf = s.stealString();	
	klist_append(&drill_list,ld);
	drill_lock.Unlock();
}
void flush_log_drill()
{
	KStringBuf drill_log_file,drill_sign;
	drill_log_file << conf.path << PATH_SPLIT_CHAR << "var" << PATH_SPLIT_CHAR << "drill.log";
	KFile fp;
	if (!fp.open(drill_log_file.getString(), fileWrite)) {
		klog(KLOG_ERR, "cann't open drill_log_file [%s] for write\n", drill_log_file.getString());
		return;
	}
	drill_lock.Lock();
	klog_drill *ld = drill_list.next;
	while (ld!=&drill_list) {
		fp.write(ld->buf, ld->len);
		klog_drill *next = ld->next;
		free_log_drill(ld);
		ld = next;
	}
	klist_init(&drill_list);
	drill_sign << "*" << drill_count << "*" << drill_hit << "*" << total_count << "*";
	fp.write(drill_sign.getString(), drill_sign.getSize());
	drill_count = 0;
	drill_hit = 0;
	total_count = 0;
	drill_lock.Unlock();
}
#endif
