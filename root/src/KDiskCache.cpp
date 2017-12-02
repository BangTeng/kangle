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
#ifndef _WIN32
#include <sys/vfs.h>
#endif
//扫描进程是否存在
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
int writeString(KFile *file,const char *str,int len)
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
		char *attr = readString(hot,hotlen,attr_len);
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
		char *val = readString(hot,hotlen,val_len);
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
bool saveCacheIndex(bool fast)
{
	//clean all DEAD obj
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
	HttpObjectIndexHeader indexHeader;
	memset(&indexHeader,0,sizeof(HttpObjectIndexHeader));
	indexHeader.head_size = sizeof(HttpObjectIndexHeader);
	indexHeader.block_size = sizeof(HttpObjectIndex);
	indexHeader.cache_dir_mask1 = CACHE_DIR_MASK1;
	indexHeader.cache_dir_mask2 = CACHE_DIR_MASK2;
	char *file_name = getCacheIndexFile();
	KFile fp;
	bool result = true;
	if(!fp.open(file_name,fileWrite)){
		klog(KLOG_ERR,"cann't open cache index file for write [%s]\n",file_name);
		xfree(file_name);
		return false;
	}
	fp.write((char *)&indexHeader,sizeof(HttpObjectIndexHeader));
	int save_count = cache.save_disk_index(&fp);
	if(result){
		//set the index file state is clean
		indexHeader.state = INDEX_STATE_CLEAN;
		indexHeader.object_count = save_count;
		fp.seek(0,seekBegin);
		fp.write((char *)&indexHeader,sizeof(HttpObjectIndexHeader));
		klog(KLOG_ERR,"total save %d object\n",save_count);
	}
	if(file_name){
		xfree(file_name);
	}
	return result;
}
cor_result create_http_object(KHttpObject *obj,const char *url,const char *verified_filename)
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
	obj->url->flags = m_url.flags;
	if (verified_filename) {
		obj->h = cache.hash_url(obj->url);
		if (cache.getHash(obj->h)->find(obj->url,verified_filename)) {
			CLR(obj->index.flags,FLAG_IN_DISK);
			klog(KLOG_NOTICE,"filename [%s] already in cache\n",verified_filename);
			return cor_incache;
		}
	}
	if (stored_obj(obj,NULL,LIST_IN_DISK)) {
		return cor_success;
	}
	CLR(obj->index.flags,FLAG_IN_DISK);
	return cor_failed;
}
cor_result create_http_object(KFile *fp,KHttpObject *obj,const char *verified_filename=NULL)
{
	int len;
	char *url = readString(fp,len);
	if(url==NULL){
		fprintf(stderr,"read url is NULL\n");
		return cor_failed;
	}
	cor_result ret = create_http_object(obj,url,verified_filename);
	free(url);
	return ret;
}
int create_file_index(const char *file,void *param)
{
	KStringBuf s;
	cor_result result = cor_failed;
	KHttpObject *obj;
	s << (char *)param << PATH_SPLIT_CHAR << file;
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
	
	KHttpObjectFileHeader header;
	if(fp.read((char *)&header,sizeof(KHttpObjectFileHeader))!=sizeof(KHttpObjectFileHeader)){
		fprintf(stderr,"cann't read head size [%s]\n",file_name);
		goto failed;
	}
	obj = new KHttpObject;
	memcpy(&obj->index,&header.index,sizeof(obj->index));
	
	result = create_http_object(&fp,obj,file_name);
	if (result==cor_success) {
#ifdef ENABLE_DB_DISK_INDEX
		if (dci) {
			dci->start(ci_add,obj);
		}
#endif
		load_count++;
#if 0
		if (rebuild_cache_hash) {
			char *file_name2 = obj->getFileName();
			if (file_name2) {
				if (strcmp(file_name,file_name2)!=0) {
					if (rename(file_name,file_name2)!=0) {
						rebuild_cache_files.insert(std::pair<std::string,std::string>(file_name,file_name2));
					}
					
				}
				free(file_name2);
			}
		}
#endif
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
void clean_disk_orphan_files()
{
	
}
void recreate_index_dir(const char *cache_dir)
{
	klog(KLOG_NOTICE,"scan cache dir [%s]\n",cache_dir);
	
	list_dir(cache_dir,create_file_index,(void *)cache_dir);
	clean_disk_orphan_files();
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
#if 0
	if (rebuild_cache_files.size()>0) {
		klog(KLOG_ERR,"now rebuild disk cache hash.\n");
		std::map<std::string,std::string>::iterator it;
		for (it=rebuild_cache_files.begin();it!=rebuild_cache_files.end();it++) {
			int ret = rename((*it).first.c_str(),(*it).second.c_str());
			if (ret!=0) {
				klog(KLOG_ERR,"cann't rename %s to %s\n",(*it).first.c_str(),(*it).second.c_str());
			}
		}
		rebuild_cache_files.clear();
	}
#endif
	klog(KLOG_ERR,"create index done. total find %d object.\n",load_count);
	index_progress = false;
}
void init_disk_cache(bool firstTime)
{
#ifdef ENABLE_SQLITE_DISK_INDEX
	if (dci==NULL) {
		memset(&index_scan_state,0,sizeof(index_scan_state));
		load_index_scan_state();
		dci = new KSqliteDiskCacheIndex;
		char *file_name = getCacheIndexFile();
		KStringBuf sqliteIndex;
		sqliteIndex << file_name << ".sqt";
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
			} else {
				m_thread.start(NULL,load_cache_index);
			}
		}
	}
#else
	memset(&index_scan_state,0,sizeof(index_scan_state));
	load_index_scan_state();
	m_thread.start(NULL,load_cache_index);
#endif
}
bool loadCacheIndex()
{
	load_count = 0;
	bool result = false;
	char *file_name = getCacheIndexFile();
#if 0
	rebuild_cache_hash = true;
#endif
	KFile fp;
	if (!fp.open(file_name,fileReadWrite)) {
		xfree(file_name);
#ifdef ENABLE_DB_DISK_INDEX
		if (dci) {
			return true;
		}
#endif
		recreate_index(time(NULL));
		return false;
	}
	HttpObjectIndexHeader indexHeader;
	if(fp.read((char *)&indexHeader,sizeof(HttpObjectIndexHeader))!=sizeof(HttpObjectIndexHeader)){
		klog(KLOG_ERR,"cann't read cache index header\n");
		recreate_index(time(NULL));
		goto done;
	}
#if 0
	if (indexHeader.cache_dir_mask1==CACHE_DIR_MASK1 
		&& indexHeader.cache_dir_mask2==CACHE_DIR_MASK2) {
		rebuild_cache_hash = false;
	}
#endif
	if(indexHeader.state != INDEX_STATE_CLEAN
#if 0
		|| rebuild_cache_hash
#endif
		){
		klog(KLOG_ERR,"cache index file not clean.\n");
		recreate_index(time(NULL));
		goto done;
	}
	for(;;){
		KHttpObject *obj = new KHttpObject;
		if (fp.read((char *)&obj->index,sizeof(HttpObjectIndex))!=sizeof(HttpObjectIndex)) {
			obj->release();
			break;
		}
		if (create_http_object(&fp,obj)==cor_success) {
			load_count++;
#ifdef ENABLE_DB_DISK_INDEX
			if (dci) {
				dci->start(ci_add,obj);
			}
#endif
		} else {
			SET(obj->index.flags,FLAG_DEAD|FLAG_IN_DISK);		
		}
		obj->release();
	}
	result = true;
	if(load_count!=indexHeader.object_count){
		klog(KLOG_ERR,"Warning not all obj have loaded,total obj count=%d,loaded=%d\n",indexHeader.object_count,load_count);
		debug("total object count=%d\n",cache.getCount());
	}
	//设置index为notclean
	//fseeko(fp,0,SEEK_SET);
	fp.seek(0,seekBegin);
	indexHeader.state = INDEX_STATE_UNCLEAN;
	fp.write((char *)&indexHeader,sizeof(HttpObjectIndexHeader));
done:
	if(file_name){
		xfree(file_name);
	}
	klog(KLOG_ERR,"total load disk obj count=%d\n",load_count);
	return result;
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
	time_t nowTime = time(NULL);
	loadCacheIndex();
	klog(KLOG_ERR,"load_cache_index use time=%d seconds\n",time(NULL)-nowTime);
	KTHREAD_RETURN;
}
void scan_disk_cache()
{
	if (index_progress) {
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

KTHREAD_FUNCTION handle_request_swapin(void *param,int msec)
{
	KHttpRequest *rq = (KHttpRequest *)param;
	KHttpObject *obj = rq->ctx->obj;
	assert(obj);
	assert(obj->data->type == SWAPING_OBJECT);
	KHttpObjectBody *osData = obj->data;
	
	assert(osData->os);
	swap_in_result result = swap_in_busy;
	KMutex *lock = obj->getLock();
	if (msec < 2000) {
		KHttpObjectBody *data = new KHttpObjectBody();
		if (obj->swapin(data)) {
			result = swap_in_success;
		} else {
			result = swap_in_failed;
		}
		/**
		* swap in进来的时候是不锁的，这样尽可能的保证畅通，因为swap in可能会比较长时间
		*/		
		lock->Lock();
		obj->data = data;
		if (result == swap_in_success) {
			SET(obj->index.flags, FLAG_IN_MEM);			
		} else {
			SET(obj->index.flags, FLAG_DEAD | OBJ_INDEX_UPDATE);
		}
		lock->Unlock();
		if (result == swap_in_success) {
			
				cache.getHash(obj->h)->incSize(obj->index.content_length);
		}
	} else {
		lock->Lock();
		obj->data = NULL;
		lock->Unlock();
	}
	assert(osData);
	osData->os->swapResult(result);
	delete osData;
	KTHREAD_RETURN;
}
#if 0
FUNC_TYPE FUNC_CALL swapin_thread(void *param)
{
	for (;;) {
		KHttpRequest *rq = NULL;
		swapinQueueLock.Lock();
		if (swapinQueue.size()<=0) {
			current_swapin_worker--;
			swapinQueueLock.Unlock();
			break;
		}
		rq = *swapinQueue.begin();
		swapinQueue.pop_front();
		swapinQueueLock.Unlock();
		handle_request_swapin(rq);
	}
	KTHREAD_RETURN;
}
#endif
void stage_swapin(KHttpRequest *rq)
{
	conf.ioWorker->start(rq,handle_request_swapin);
	/*
	swapinQueueLock.Lock();
	swapinQueue.push_back(rq);
	if (current_swapin_worker>=MAX_SWAPIN_WORKER) {
		swapinQueueLock.Unlock();
		return;
	}
	current_swapin_worker++;	
	swapinQueueLock.Unlock();
	if (!m_thread.start(NULL,swapin_thread)) {
		swapinQueueLock.Lock();
		current_swapin_worker--;
		swapinQueueLock.Unlock();
	}
	*/
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
#else
	struct statfs buf;
	if (statfs(path.c_str(), &buf) != 0) {
		return false;
	}
	total_size = (INT64)buf.f_blocks * (INT64)buf.f_bsize;
	free_size = (INT64)buf.f_bsize * (INT64)buf.f_bavail;
#endif
	return true;
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
