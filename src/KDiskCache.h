#ifndef KDiskCache_h_0s9dflk1j231hhhh1
#define KDiskCache_h_0s9dflk1j231hhhh1
#include "global.h"
#include "forwin32.h"
#include "KHttpRequest.h"
#include "KFile.h"
#include <stdio.h>
#include "KAsyncFile.h"
#include <list>
#define INDEX_STATE_UNCLEAN 0
#define INDEX_STATE_CLEAN   1
#define CACHE_DISK_VERSION  3
#define CACHE_FIX_STR      "KLJW"

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
struct KHttpObjectKey
{
	unsigned filename1;//从kgl_current_sec得到
	unsigned filename2;//每次累加
};
struct HttpObjectIndex
{
	unsigned head_size;
	unsigned flags;
	INT64 content_length; //obj的总长度
	time_t last_modified;
	time_t last_verified;	
	unsigned max_age;
};
struct KHttpObjectFileHeader
{
	char fix_str[4];
	u_short version;
	u_short url_flag_encoding;
	HttpObjectIndex index;
	unsigned short body_complete;
	unsigned short status_code;	
};
struct KHttpObjectDbIndex
{
	HttpObjectIndex index;
	u_short url_flag_encoding;
	u_short reserv;
};
struct index_scan_state_t
{
	int first_index;
	int second_index;
	int need_index_progress;
	time_t last_scan_time;
};
inline bool is_valide_dc_head_size(unsigned head_size)
{
	return head_size >= sizeof(KHttpObjectFileHeader) && head_size < 4048576;
}
inline bool is_valide_dc_sign(KHttpObjectFileHeader *header)
{
	return memcmp(header->fix_str, CACHE_FIX_STR, sizeof(header->fix_str)) == 0 &&
		header->version== CACHE_DISK_VERSION;
}
bool skipString(char **hot,int &hotlen);
char *readString(char **hot,int &hotlen,int &len);
bool skipString(KFile *file);
int writeString(KBufferFile *fp,const char *str,int len=0);
int write_string(char *hot, const char *str, int len);
char *getCacheIndexFile();
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
cor_result create_http_object(KHttpObject *obj,const char *url, u_short flag_encoding,const char *verified_filename=NULL);
extern volatile bool index_progress;
extern index_scan_state_t index_scan_state;
#endif
