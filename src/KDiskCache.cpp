#include <string.h>
#include <stdlib.h>
#include <string>

#include "KDiskCache.h"
#include "do_config.h"
#include "directory.h"
#include "cache.h"
#include "lib.h"
#include "KHttpObject.h"
#include "KHttpObjectHash.h"
#include "malloc_debug.h"
#include "KObjectList.h"
#include "http.h"
#include "KFile.h"
#ifdef ENABLE_DB_DISK_INDEX
#include "KDiskCacheIndex.h"
#include "KSqliteDiskCacheIndex.h"
#endif
#ifdef ENABLE_DISK_CACHE
#ifdef LINUX
#include <sys/vfs.h>
#endif
//ɨ������Ƿ����
volatile bool index_progress = false;
index_scan_state_t index_scan_state;
static int load_count = 0;
KMutex swapinQueueLock;
std::list<KHttpRequest *> swapinQueue;
static INT64 recreate_start_time = 0;

#if 0
static bool rebuild_cache_hash = false;
static std::map<std::string,std::string> rebuild_cache_files;
#endif
using namespace std;
bool skipString(char **hot,int &hotlen)
{
	if (hotlen<=(int)sizeof(int)) {
		return false;
	}
	int len;
	memcpy(&len,*hot,sizeof(int));
	(*hot)+=sizeof(int);
	hotlen-=sizeof(int);
	if (hotlen<=len) {
		return false;
	}
	(*hot) += len;
	hotlen -= len;
	return true;
}
bool skipString(KFile *file)
{
	int len;
	if(file->read((char *)&len,sizeof(len))!=sizeof(len)){
		return false;
	}
	return file->seek(len,seekCur);
}
char *readString(char **hot,int &hotlen,int &len)
{
	if (hotlen<(int)sizeof(int)) {
		len = -1;
		return NULL;
	}
	memcpy(&len,*hot,sizeof(int));
	hotlen-=sizeof(int);
	(*hot) += sizeof(int);
	if(len<0 || len>1000000){
		klog(KLOG_ERR,"string len[%d] is too big\n",len);
		len = -1;
		return NULL;
	}
	if (hotlen<len) {
		len = -1;
		return NULL;
	}
	char *buf = (char *)xmalloc(len+1);
	buf[len]='\0';
	if (len>0) {
		memcpy(buf,*hot,len);
		hotlen -= len;
		(*hot) += len;
	}
	return buf;
}
char *readString(KFile *file,int &len)
{
	if(file->read((char *)&len,sizeof(len))!=sizeof(len)){
		len = -1;
		return NULL;
	}
	if(len<0 || len>1000000){
		len = -1;
		klog(KLOG_ERR,"string len[%d] is too big\n",len);
		return NULL;
	}
	char *buf = (char *)xmalloc(len+1);
	buf[len]='\0';
	if (len>0 && (int)file->read(buf,len)!=len) {
		xfree(buf);
		len = -1;
		return NULL;
	}
	return buf;
}
int write_string(char *hot, const char *str, int len)
{
	memcpy(hot, (char *)&len, sizeof(int));
	if (len > 0) {
		memcpy(hot + sizeof(int), str, len);
	}
	return len + sizeof(int);
}
int writeString(KBufferFile *file,const char *str,int len)
{
	if (str) {
		if (len==0) {
			len = strlen(str);
		}
	}
	int ret = file->write((char *)&len,sizeof(len));
	if (len>0) {
		ret += file->write(str,len);
	}
	return ret;
}
bool read_obj_head(KHttpObjectBody *data,char **hot,int &hotlen)
{
	assert(data->headers==NULL);
	KHttpHeader *last = NULL;
	for (;;) {
		int attr_len,val_len;
		//printf("before attr hotlen=[%d]\n",hotlen);
		char *attr = readString(hot,hotlen,attr_len);
		//printf("after attr before val hotlen=[%d]\n",hotlen);
		if (attr_len==-1) {
			return false;
		}
		if (attr==NULL) {
			return true;
		}
		if (*attr=='\0') {
			free(attr);
			return true;
		}
		//printf("attr=[%s]\n",attr);
		char *val = readString(hot,hotlen,val_len);
		//printf("after val hotlen=[%d]\n",hotlen);
		if(val_len==-1){
			xfree(attr);
			return false;
		}
		//printf("val=[%s]\n",val);
		KHttpHeader *header = (KHttpHeader *)xmalloc(sizeof(KHttpHeader));
		if(header==NULL){
			xfree(attr);
			if(val){
				xfree(val);
			}
			return false;
		}
		header->attr = attr;
		header->val = val;
		header->next = NULL;
		header->attr_len = attr_len;
		header->val_len = val_len;
		if(last==NULL){
			data->headers = header;		
		}else{
			last->next = header;
		}
		last = header;
	}
}
bool read_obj_head(KHttpObjectBody *data,KFile *fp)
{
	assert(data->headers==NULL);
	KHttpHeader *hot = NULL;
	for (;;) {
		int attr_len,val_len;
		char *attr = readString(fp,attr_len);
		if (attr_len == -1) {
			return false;
		}
		if (attr==NULL) {
			return true;
		}
		if (*attr=='\0') {
			free(attr);
			return true;
		}
		char *val = readString(fp,val_len);
		if(val_len==-1){
			xfree(attr);
			return false;
		}
		KHttpHeader *header = (KHttpHeader *)xmalloc(sizeof(KHttpHeader));
		if(header==NULL){
			xfree(attr);
			if(val){
				xfree(val);
			}
			return false;
		}
		header->attr = attr;
		header->val = val;
		header->attr_len = attr_len;
		header->val_len = val_len;
		header->next = NULL;
		if(hot==NULL){
			data->headers = header;		
		}else{
			hot->next = header;
		}
		hot = header;
	}
}
char *getCacheIndexFile()
{
	KStringBuf s;
	if (*conf.disk_cache_dir) {
		s << conf.disk_cache_dir;
	} else {
		s << conf.path << "cache" << PATH_SPLIT_CHAR ;
	}
	s << "index";
	return s.stealString();
}
void get_index_scan_state_filename(KStringBuf &s)
{
	if (*conf.disk_cache_dir) {
		s << conf.disk_cache_dir;
	} else {
		s << conf.path << "cache" << PATH_SPLIT_CHAR ;
	}
	s << "index.scan";
}
bool save_index_scan_state()
{
	KStringBuf s;
	get_index_scan_state_filename(s);
	KFile fp;
	if (!fp.open(s.getString(),fileWrite)) {
		return false;
	}
	bool result = true;
	if (sizeof(index_scan_state_t) != fp.write((char *)&index_scan_state,sizeof(index_scan_state_t))) {
		result = false;
	}
	fp.close();
	return result;
}
bool load_index_scan_state()
{
	KStringBuf s;
	get_index_scan_state_filename(s);
	KFile fp;
	if (!fp.open(s.getString(),fileRead)) {
		return false;
	}
	bool result = true;
	if (sizeof(index_scan_state_t) != fp.read((char *)&index_scan_state,sizeof(index_scan_state_t))) {
		result = false;
	}
	fp.close();
	return result;
}
int get_index_scan_progress()
{
	return (index_scan_state.first_index * 100) / CACHE_DIR_MASK1;
}
bool saveCacheIndex()
{
	klog(KLOG_ERR, "save cache index now...\n");
#ifndef NDEBUG
	cache.flush_mem_to_disk();
#endif
	cache.syncDisk();
#ifdef ENABLE_DB_DISK_INDEX
	if (dci) {
		dci->start(ci_close,NULL);
		while (!dci->allWorkedDone()) {
			my_msleep(100);
		}
		return false;
	}
#endif
	return false;
}
cor_result create_http_object(KHttpObject *obj,const char *url,u_short flag_encoding,const char *verified_filename)
{
	KUrl m_url;
	if (!parse_url(url,&m_url) || m_url.host==NULL) {
		fprintf(stderr,"cann't parse url[%s]\n",url);
		m_url.destroy();
		return cor_failed;
	}
	CLR(obj->index.flags,FLAG_IN_MEM);
	SET(obj->index.flags,FLAG_IN_DISK|FLAG_URL_FREE);
	obj->url = new KUrl;
	obj->url->host = m_url.host;
	obj->url->path = m_url.path;
	obj->url->param = m_url.param;
	obj->url->port = m_url.port;
	obj->url->flag_encoding = flag_encoding;
	if (verified_filename) {
		obj->h = cache.hash_url(obj->url);
		if (cache.getHash(obj->h)->find(obj->url,verified_filename)) {
			CLR(obj->index.flags,FLAG_IN_DISK);
			klog(KLOG_NOTICE,"filename [%s] already in cache\n",verified_filename);
			return cor_incache;
		}
	}
	if (!TEST(obj->index.flags, OBJ_INDEX_SAVED)) {
		SET(obj->index.flags, FLAG_DEAD);
	}
	if (stored_obj(obj,LIST_IN_DISK)) {
		return cor_success;
	}
	CLR(obj->index.flags,FLAG_IN_DISK);
	return cor_failed;
}
cor_result create_http_object(KFile *fp,KHttpObject *obj,u_short url_flag_encoding,const char *verified_filename=NULL)
{
	int len;
	char *url = readString(fp,len);
	if(url==NULL){
		fprintf(stderr,"read url is NULL\n");
		return cor_failed;
	}
	cor_result ret = create_http_object(obj,url, url_flag_encoding,verified_filename);
	free(url);
	return ret;
}
int create_file_index(const char *file,void *param)
{
	KStringBuf s;
	cor_result result = cor_failed;
	KHttpObject *obj;
	s << (char *)param << PATH_SPLIT_CHAR << file;
	unsigned f1 = 0;
	unsigned f2 = 0;
	char *file_name = s.getString();
	KFile fp;
	if (!fp.open(s.getString(),fileRead)) {
		fprintf(stderr,"cann't open file[%s]\n",s.getString());
		return 0;
	}
	if (recreate_start_time>0) {
		INT64 t = fp.getCreateTime();
		if (t>recreate_start_time) {
			klog(KLOG_DEBUG,"file [%s] is new file t=%d\n",file_name,(int)(t-recreate_start_time));
			return 0;
		}
	}
	
	if (sscanf(file, "%x_%x", &f1, &f2)!=2) {
		goto failed;
	}
	KHttpObjectFileHeader header;
	if(fp.read((char *)&header,sizeof(KHttpObjectFileHeader))!=sizeof(KHttpObjectFileHeader)){
		fprintf(stderr,"cann't read head size [%s]\n",file_name);
		goto failed;
	}
	if (!is_valide_dc_head_size(header.index.head_size)) {
		klog(KLOG_ERR, "disk cache [%s] head_size=[%d] is not valide\n", file_name, header.index.head_size);
		goto failed;
	}
	if (!is_valide_dc_sign(&header)) {
		klog(KLOG_ERR, "disk cache [%s] is not valide file\n", file_name);
		goto failed;
	}
	obj = new KHttpObject;
	memcpy(&obj->index,&header.index,sizeof(obj->index));
	obj->dk.filename1 = f1;
	obj->dk.filename2 = f2;
	
	result = create_http_object(&fp,obj,header.url_flag_encoding,file_name);
	if (result==cor_success) {
#ifdef ENABLE_DB_DISK_INDEX
		if (dci) {
			dci->start(ci_add,obj);
		}
#endif
		load_count++;
	}
	obj->release();
failed:
	fp.close();
	if (result==cor_failed) {
		klog(KLOG_NOTICE,"create http object failed,remove file[%s]\n",file_name);
		unlink(file_name);		
	}
	return 0;
}
void clean_disk_orphan_files(const char *cache_dir)
{
	
}
void recreate_index_dir(const char *cache_dir)
{
	klog(KLOG_NOTICE,"scan cache dir [%s]\n",cache_dir);
	
	list_dir(cache_dir,create_file_index,(void *)cache_dir);
	clean_disk_orphan_files(cache_dir);
}
bool recreate_index(const char *path,int &first_dir_index,int &second_dir_index,KTimeMatch *tm=NULL)
{
	KStringBuf s;
	for (;first_dir_index<=CACHE_DIR_MASK1;first_dir_index++) {
		for (;second_dir_index<=CACHE_DIR_MASK2;second_dir_index++) {
			if (tm && !tm->checkTime(time(NULL))) {
				return false;
			}
			s << path;
			s.addHex(first_dir_index);
			s << PATH_SPLIT_CHAR;
			s.addHex(second_dir_index);
			recreate_index_dir(s.getString());
			s.clean();
			if (tm) {
				save_index_scan_state();
			}
		}
		second_dir_index = 0;
		if (tm) {
			save_index_scan_state();
		}
	}
	return true;
}
void recreate_index(time_t start_time)
{
	if (index_progress) {
		return;
	}
	recreate_start_time = start_time;
	index_progress = true;
	klog(KLOG_ERR,"now recreate the index file,It may be use more time.Please wait...\n");
	string path;
	if (*conf.disk_cache_dir) {
		path = conf.disk_cache_dir;
	} else {
		path = conf.path;
		path += "cache";
		path += PATH_SPLIT_CHAR;
	}
	KStringBuf s;
	load_count=0;
	int i=0;
	int j=0;
	recreate_index(path.c_str(),i,j,NULL);
	klog(KLOG_ERR,"create index done. total find %d object.\n",load_count);
	index_progress = false;
}
void init_disk_cache(bool firstTime)
{
	//printf("sizeof(KHttpObjectFileHeader)=%d\n", sizeof(KHttpObjectFileHeader));
#ifdef ENABLE_SQLITE_DISK_INDEX
	if (dci) {
		return;
	}
	memset(&index_scan_state,0,sizeof(index_scan_state));
	load_index_scan_state();
	dci = new KSqliteDiskCacheIndex;
	char *file_name = getCacheIndexFile();
	KStringBuf sqliteIndex;
	//remove old version sqt
	bool remove_old_index = false;
	for (int i = 1; i < CACHE_DISK_VERSION; i++) {
		sqliteIndex << file_name << ".sqt";
		if (i > 1) {
			sqliteIndex << i;
		}
		if (0 == unlink(sqliteIndex.getString())) {
			remove_old_index = true;
		}
		sqliteIndex.clean();
	}
	if (remove_old_index) {
		rescan_disk_cache();
	}
	sqliteIndex << file_name << ".sqt" << CACHE_DISK_VERSION;
	free(file_name);
	KFile fp;
	if (fp.open(sqliteIndex.getString(),fileRead)) {
		fp.close();
		if (dci->open(sqliteIndex.getString())) {
			dci->start(ci_load,NULL);
		} else {
			klog(KLOG_ERR,"recreate the disk cache index database\n");
			dci->close();
			unlink(sqliteIndex.getString());
			dci->create(sqliteIndex.getString());
			rescan_disk_cache();
		}
	} else {
		if (!dci->create(sqliteIndex.getString())) {
			delete dci;
			dci = NULL;
		}
	}
#else
	memset(&index_scan_state,0,sizeof(index_scan_state));
	load_index_scan_state();
	m_thread.start(NULL,load_cache_index);
#endif
}
FUNC_TYPE FUNC_CALL scan_disk_cache_thread(void *param)
{
#if 0
	rebuild_cache_hash = true;
#endif
	string path;
	if (*conf.disk_cache_dir) {
		path = conf.disk_cache_dir;
	} else {
		path = conf.path;
		path += "cache";
		path += PATH_SPLIT_CHAR;
	}
	if (recreate_index(path.c_str(),index_scan_state.first_index,index_scan_state.second_index,&conf.diskWorkTime)) {
		index_scan_state.need_index_progress = 0;
		index_scan_state.last_scan_time = time(NULL);
		save_index_scan_state();
	}
	index_progress = false;
#if 0
	rebuild_cache_hash = false;
#endif
	KTHREAD_RETURN;
}
FUNC_TYPE FUNC_CALL load_cache_index(void *param)
{
	//time_t nowTime = time(NULL);
	//loadCacheIndex();
	//klog(KLOG_ERR,"load_cache_index use time=%d seconds\n",time(NULL)-nowTime);
	KTHREAD_RETURN;
}
void scan_disk_cache()
{
	if (index_progress || cache.is_disk_shutdown()) {
		return;
	}
	if (!index_scan_state.need_index_progress) {
		return;
	}
	if (!conf.diskWorkTime.checkTime(time(NULL))) {
		return;
	}
	index_progress = true;
	if (!m_thread.start(NULL,scan_disk_cache_thread)) {
		index_progress = false;
	}
}

void rescan_disk_cache()
{
	index_scan_state.first_index = 0;
	index_scan_state.second_index = 0;
	index_scan_state.need_index_progress = 1;
	save_index_scan_state();
}
bool get_disk_size(INT64 &total_size,INT64 &free_size) {
	string path;
	if (*conf.disk_cache_dir) {
		path = conf.disk_cache_dir;
	} else {
		path = conf.path;
		path += "cache";
		path += PATH_SPLIT_CHAR;
	}
#ifdef _WIN32
	ULARGE_INTEGER FreeBytesAvailable, TotalNumberOfBytes, TotalNumberOfFreeBytes;
	if (!GetDiskFreeSpaceEx(path.c_str(), &FreeBytesAvailable, &TotalNumberOfBytes, &TotalNumberOfFreeBytes)) {
		return false;
	}
	total_size = TotalNumberOfBytes.QuadPart;
	free_size = FreeBytesAvailable.QuadPart;
	return true;
#elif LINUX
	struct statfs buf;
	if (statfs(path.c_str(), &buf) != 0) {
		return false;
	}
	total_size = (INT64)buf.f_blocks * (INT64)buf.f_bsize;
	free_size = (INT64)buf.f_bsize * (INT64)buf.f_bavail;
	return true;
#endif
	return false;
}
INT64 get_need_free_disk_size(int used_radio)
{
	if (used_radio>=95) {
		used_radio = 95;
	}
	if (used_radio<=0) {
		used_radio = 1;
	}
	INT64 total_size = 0;
	INT64 free_size = 0;
	if (!get_disk_size(total_size, free_size)) {
		return 0;
	}
	INT64 min_free_size = total_size * (100 - used_radio) / 100;
	return min_free_size - free_size;
}
#endif
