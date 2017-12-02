#include <string.h>
#include <stdlib.h>
#include <vector>
#ifdef _WIN32
#include <direct.h>
#endif
#include "KDiskCache.h"
#include "do_config.h"
#include "utils.h"
#include "cache.h"
#include "KHttpObject.h"
#include "KHttpObjectHash.h"
#ifdef ENABLE_DB_DISK_INDEX
#include "KDiskCacheIndex.h"
#endif
#include "forwin32.h"
#include "malloc_debug.h"
KMutex hash_lock[HASH_SIZE+1];
static unsigned file_index = (rand() & CACHE_DIR_MASK2);
static KMutex indexLock;
KHttpObjectBody::KHttpObjectBody(KHttpObjectBody *data)
{
	memset(this, 0, sizeof(KHttpObjectBody));
	status_code = data->status_code;	
	KHttpHeader *hot = headers;
	KHttpHeader *tmp = data->headers;
	while (tmp) {
		KHttpHeader *new_t = (KHttpHeader *)xmalloc(sizeof(KHttpHeader));
		new_t->attr = xstrdup(tmp->attr);
		if(tmp->val){
			new_t->val = xstrdup(tmp->val);
		}else{
			new_t->val = NULL;
		}
		new_t->attr_len = tmp->attr_len;
		new_t->val_len = tmp->val_len;
		new_t->next = NULL;
		if(hot==NULL){
			headers = new_t;
		}else{
			hot->next = new_t;
		}
		hot = new_t;
		tmp = tmp->next;
	}
}
KHttpObject::KHttpObject(KHttpObject *obj)
{	
	init(obj->url);
	index.flags = obj->index.flags;
	CLR(index.flags,FLAG_IN_DISK|FLAG_URL_FREE|OBJ_IS_READY|OBJ_IS_STATIC2|OBJ_IS_DELTA|FLAG_NO_BODY|ANSW_HAS_CONTENT_LENGTH|ANSW_HAS_CONTENT_RANGE|FLAG_RQ_GZIP);
	SET(index.flags,FLAG_IN_MEM);
	index.last_verified = obj->index.last_verified;
	index.last_modified = obj->index.last_modified;
	index.max_age = obj->index.max_age;
	data = new KHttpObjectBody(obj->data);
}
KHttpObject::~KHttpObject() {
	unlinkDiskFile();
	if (data) {
		delete data;
	}
	if (url && TEST(index.flags,FLAG_URL_FREE)) {
		//url由obj负责，则清理
		url->destroy();
		delete url;
	}
}
void KHttpObject::unlinkDiskFile()
{
#ifdef ENABLE_DISK_CACHE
	if (TEST(index.flags,FLAG_IN_DISK)) {
#ifdef ENABLE_DB_DISK_INDEX
		if (dci) {
			dci->start(ci_del,this);
		}
#endif
		char *name = getFileName();
		if (0!=unlink(name)) {
			klog(KLOG_ERR,"cann't unlink file=[%s] errno=%d\n",name,errno);
		}
		
		free(name);
	}
#endif
}
#ifdef ENABLE_DISK_CACHE
char *KHttpObject::getFileName(bool part)
{	
	KStringBuf s;
	if (*conf.disk_cache_dir) {
		s << conf.disk_cache_dir;
	} else {
		s << conf.path << "cache" << PATH_SPLIT_CHAR;
	}
	if (index.filename1==0) {
		index.filename1 = (unsigned) kgl_current_sec;
		indexLock.Lock();
		index.filename2 = file_index++;
		indexLock.Unlock();
	}
	s.addHex((index.filename1 & CACHE_DIR_MASK1));
	s << PATH_SPLIT_CHAR;
	s.addHex((index.filename2 & CACHE_DIR_MASK2));
	s << PATH_SPLIT_CHAR;
	s.addHex(index.filename1);
	s << "_";
	s.addHex(index.filename2);
	if (part) {
		s << ".part";
	}
	return s.stealString();
}
#endif
int KHttpObject::saveIndex(KFile *fp)
{
#ifdef ENABLE_DISK_CACHE
	assert(TEST(index.flags,FLAG_IN_DISK));
	//assert(index.file_name>0);
	int len = sizeof(index);
	if(fp->write((char *)&index,len)!=len){
		return -1;
	}
	char *urlstr = url->getUrl();
	int ret = writeString(fp,urlstr);
	xfree(urlstr);
	if (ret<0) {
		return -1;
	}
	len += ret;
	return len;
#endif
	return -1;

}
int KHttpObject::saveHead(KFile *fp)
{
#ifdef ENABLE_DISK_CACHE
	KHttpObjectFileHeader fileHeader;
	int bodyStart = sizeof(KHttpObjectFileHeader);
	memset(&fileHeader,0,sizeof(fileHeader));
	memcpy(&fileHeader.index,&index,sizeof(HttpObjectIndex));
	KHttpHeader *header = data->headers;
	fp->seek(bodyStart,seekBegin);
	char *u = url->getUrl();
	bodyStart += writeString(fp,u);
	free(u);
	while (header) {
		bodyStart += writeString(fp,header->attr);
		bodyStart += writeString(fp,header->val);
		header = header->next;
	}
	bodyStart += writeString(fp,NULL);
	fileHeader.head_size = bodyStart;
	memcpy(fileHeader.fix_str,CACHE_FIX_STR,sizeof(CACHE_FIX_STR));
	
	fileHeader.body_complete = 1;
	
	fp->seek(0,seekBegin);
	fp->write((char *)&fileHeader,sizeof(KHttpObjectFileHeader));
	return bodyStart;
#endif
	return -1;
}
bool KHttpObject::swapout()
{
#ifdef ENABLE_DISK_CACHE
	if (conf.disk_cache<=0) {
		return false;
	}
	KFile file;
	buff *tmp;
	char *filename = NULL;
	int body_size;
	assert(data);
	if (TEST(index.flags,FLAG_IN_DISK)) {
#ifdef ENABLE_DB_DISK_INDEX
		if (TEST(index.flags,OBJ_INDEX_UPDATE)) {
			//长度有污染
			CLR(index.flags,OBJ_INDEX_UPDATE);
			if (dci) {
				dci->start(ci_update,this);
			}
		}
#endif
		return true;
	}
	filename=getFileName();
	if (!file.open(filename,fileWrite)) {
		klog(KLOG_WARNING,"Cann't open file %s to write.\n",filename);
		goto swap_out_failed;
	}
	klog(KLOG_DEBUG, "Now swap out obj %s:%d%s\n",url->host,url->port,url->path);
	body_size = saveHead(&file);
	file.seek(body_size,seekBegin);
	assert(data->type==MEMORY_OBJECT);	
	tmp = data->bodys;
	while (tmp) {
		if (file.write(tmp->data, tmp->used)<(int)tmp->used) {
			klog(KLOG_ERR,"Cann't write file %s to disk.\n",filename);
			goto swap_out_failed;
		}
		tmp=tmp->next;
	}
	
		cache.getHash(h)->incDiskSize(index.content_length);
	if (filename) {
		free(filename);
	}
#ifdef ENABLE_DB_DISK_INDEX
	if (dci) {
		dci->start(ci_add,this);
	}
#endif
	return true;
swap_out_failed:
	if (file.opened()) {
		file.close();
		unlink(filename);
	}
	if (filename) {
		free(filename);
	}
#endif
	return false;
}
bool KHttpObject::swapinBody(KFile *fp,KHttpObjectBody *data)
{
	assert(data->bodys==NULL && data->type==MEMORY_OBJECT);
	INT64 left_read = index.content_length;
	buff *last = NULL;
	while (left_read>0) {
		int this_read = (int)MIN(left_read,65536);
		char *buf = (char *)xmalloc(this_read);
		if (buf==NULL) {
			return false;
		}
		if (fp->read(buf,this_read)!=this_read) {
			free(buf);
			return false;
		}
		buff *tmp = (buff *)malloc(sizeof(buff));
		tmp->used = this_read;
		tmp->data = buf;
		tmp->flags = 0;
		tmp->next = NULL;
		if (last==NULL) {
			data->bodys = tmp;
		} else {
			last->next = tmp;
		}
		last = tmp;
		left_read-=this_read;
	}
	return true;
}
bool KHttpObject::swapin(KHttpObjectBody *data)
{	
	bool result = false;
#ifdef ENABLE_DISK_CACHE
	assert(0==TEST(index.flags,FLAG_IN_MEM));
	assert(data->bodys==NULL);
	KHttpObjectFileHeader fileHeader;
	char *tmpbuf = NULL;
	int hotlen;
	char *hot = NULL;
	char *filename = getFileName();
	if (filename==NULL) {
		return false;
	}
	KFile fp;
	if (!fp.open(filename,fileRead,0)) {
		free(filename);
		return false;
	}
	if (fp.read((char *)&fileHeader,sizeof(KHttpObjectFileHeader))!=sizeof(KHttpObjectFileHeader)) {
		goto failed;
	}
	if (memcmp(fileHeader.fix_str,CACHE_FIX_STR,sizeof(fileHeader.fix_str))!=0) {
		goto failed;
	}
	data->headerSize = fileHeader.head_size;
	if (data->headerSize > 4048576) {
		//头文件太大了
		goto failed;
	}
	hotlen = data->headerSize - sizeof(KHttpObjectFileHeader);
	tmpbuf = (char *)malloc(hotlen+1);
	hot = tmpbuf;	
	if (hotlen!=fp.read(tmpbuf,hotlen)) {
		goto failed;
	}	
	//skip url
	skipString(&hot,hotlen);
	//skipString(&fp);
	if (!read_obj_head(data,&hot,hotlen)) {
		goto failed;
	}
	//assert(hotlen==0);
	if (fileHeader.body_complete) {
		
			//load memory object
			if (!swapinBody(&fp,data)) {
				goto failed;
			}
			result = true;
			goto success;
		
	}
	
failed:
	fp.close();
	if (unlink(filename)!=0) {
		klog(KLOG_WARNING,"cann't unlink corrupt object file [%s]\n",filename);
	}
	if (data->headers) {
		free_header(data->headers);
		data->headers = NULL;
	}
success:
	if (tmpbuf) {
		free(tmpbuf);
	}	
	free(filename);
#endif
	return result;	
}
