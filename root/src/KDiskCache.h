#ifndef KDiskCache_h_0s9dflk1j231hhhh1
#define KDiskCache_h_0s9dflk1j231hhhh1
#include "global.h"
#include "forwin32.h"
#include "KHttpRequest.h"
#include "KFile.h"
#include <stdio.h>
#include <list>
#define INDEX_STATE_UNCLEAN 0
#define INDEX_STATE_CLEAN   1
#define CURRENT_DISK_VERSION 1
#define CACHE_FIX_STR "KLJW"
enum swap_in_result {
	swap_in_success,
	swap_in_failed,
	swap_in_busy
};
typedef void (*swap_http_obj_call_back)(KHttpRequest *rq, swap_in_result result);
class KHttpObjectSwapTask {
public:
	KHttpRequest *rq;
	swap_http_obj_call_back cb;
	KHttpObjectSwapTask *next;
};
class KHttpObjectSwaping
{
public:
	KHttpObjectSwaping()
	{
		queue = NULL;
	}
	~KHttpObjectSwaping()
	{
		assert(queue==NULL);
	}
	void addTask(KHttpRequest *rq,swap_http_obj_call_back cb)
	{
		KHttpObjectSwapTask *task = new KHttpObjectSwapTask();
		task->rq = rq;
		task->cb = cb;
		task->next = queue;
		queue = task;
	}
	void swapResult(swap_in_result result)
	{
		KHttpObjectSwapTask *next;
		while (queue) {
			next = queue->next;
			queue->cb(queue->rq,result);
			delete queue;
			queue = next;
		}
	}
private:
	KHttpObjectSwapTask *queue;
};
struct HttpObjectIndexHeader
{
	int head_size;
	int version;
	int object_count;
	int state;
	int block_size;
	union {
		char reserv[8];
		struct {
			short cache_dir_mask1;
			short cache_dir_mask2;
			char reserv2[4];
		};
	};
};
struct HttpObjectIndex
{
	unsigned filename1;//从kgl_current_sec得到
	unsigned filename2;//每次累加
	INT64 content_length; //obj的总长度
	INT64 have_length;    //kangle已有的大小
	time_t last_modified;
	time_t last_verified;
	unsigned flags;
	unsigned max_age;
};
struct KHttpObjectFileHeader
{
	int head_size;
	unsigned short version;
	unsigned short body_complete;
	char fix_str[4];
	char reserv[8];
	HttpObjectIndex index;
};
struct index_scan_state_t
{
	int first_index;
	int second_index;
	int need_index_progress;
	time_t last_scan_time;
};
bool skipString(char **hot,int &hotlen);
char *readString(char **hot,int &hotlen,int &len);
bool skipString(KFile *file);
int writeString(KFile *fp,const char *str,int len=0);
char *getCacheIndexFile();
void stage_swapin(KHttpRequest *rq);
class KHttpObjectBody;
bool read_obj_head(KHttpObjectBody *data,KFile *fp);
bool read_obj_head(KHttpObjectBody *data,char **hot,int &hotlen);
void scan_disk_cache();
bool get_disk_size(INT64 &total_size, INT64 &free_size);
INT64 get_need_free_disk_size(int used_radio);
void init_disk_cache(bool firstTime);
int get_index_scan_progress();
bool save_index_scan_state();
bool load_index_scan_state();
void rescan_disk_cache();
enum cor_result
{
	cor_failed,
	cor_success,
	cor_incache,
};
cor_result create_http_object(KHttpObject *obj,const char *url,const char *verified_filename=NULL);
extern volatile bool index_progress;
extern index_scan_state_t index_scan_state;
#endif
