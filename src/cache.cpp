/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 *
 * kangle web server              http://www.kangleweb.net/
 * ---------------------------------------------------------------------
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *  See COPYING file for detail.
 *
 *  Author: KangHongjiu <keengo99@gmail.com>
 */
#include "global.h"

#include <string.h>
#include <stdlib.h>
#include <vector>
#include "http.h"
#include "log.h"
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <sstream>
#include "cache.h"
#include "malloc_debug.h"
#include "KHttpObjectHash.h"
#include "KThreadPool.h"
#include "KObjectList.h"
#include "KCache.h"
#include "KSimulateRequest.h"
using namespace std;
void dump_object(KHttpObject *obj) {

}
int clean_cache(KReg *reg,int flag)
{
	return cache.clean_cache(reg,flag);
}
void dead_all_obj()
{
	cache.dead_all_obj();
}
void clean_static_obj_header(KHttpObject *obj) {
	assert(obj->data);
	KHttpHeader *h = obj->data->headers;
	KHttpHeader *last = NULL;
	while (h) {
		KHttpHeader *next = h->next;
		if (strcasecmp(h->attr,"Set-Cookie")==0
			|| strcasecmp(h->attr,"Set-Cookie2")==0 ) {
			if (last) {
				last->next = next;
			} else {
				obj->data->headers = next;
			}
			free(h->attr);
			free(h->val);
			free(h);
			h = next;
			continue;
		}
		last = h;
		h = next;
	}
}
bool stored_obj(KHttpObject *obj, int list_state) {
#ifdef ENABLE_DISK_CACHE
	if (TEST(obj->index.flags, FLAG_IN_DISK) &&
		obj->index.head_size != kgl_align(obj->index.head_size, kgl_aio_align_size)) {
		char *url = obj->url->getUrl();
		char *filename = obj->getFileName();
		klog(KLOG_ERR, "disk cache file head_size=[%d] is not align by size=[%d], url=[%s] file=[%s] now dead it.\n", obj->index.head_size, kgl_aio_align_size, url, filename);
		free(filename);
		free(url);
		SET(obj->index.flags, FLAG_DEAD);
	}
#endif
	return cache.add(obj,list_state);
}
bool stored_obj(KHttpRequest *rq, KHttpObject *obj,KHttpObject *old_obj) {
	//	printf("try stored obj now,path=%s\n",obj->url.path);
	if (obj == NULL){
		return false;
	}
	if (!TEST(obj->index.flags,OBJ_IS_READY)) {
		return false;
	}
	if (!objCanCache(rq,obj)) {
		return false;
	}
	/*
	if (TEST(rq->flags,RQ_HAS_GZIP)) {
		SET(obj->index.flags,FLAG_RQ_GZIP);
	}
	*/
	if (TEST(rq->filter_flags, RF_NO_DISK_CACHE)) {
		SET(obj->index.flags, FLAG_NO_DISK_CACHE);
	}
	if (TEST(rq->workModel,WORK_MODEL_INTERNAL)) {
		SET(obj->index.flags,FLAG_RQ_INTERNAL);
	}
	if (TEST(obj->index.flags,OBJ_IS_STATIC2)) {
		clean_static_obj_header(obj);
	}
#if 0
	if (obj->data && obj->data->type == SAVING_OBJECT && obj->data->fp) {
		obj->saveFileIndex(obj->data->fp);
		obj->data->fp->close();
		delete obj->data->fp;
		obj->data->fp = NULL;
		obj->data->type = MEMORY_OBJECT;
		if (dci) {
			dci->start(ci_add, obj);
		}
	}
#endif
	if (old_obj) {
		SET(old_obj->index.flags, FLAG_DEAD|OBJ_INDEX_UPDATE );
	}
	if (stored_obj(obj,(TEST(obj->index.flags,FLAG_IN_MEM)?LIST_IN_MEM:LIST_IN_DISK))) {
		SET(rq->flags,RQ_OBJ_STORED);
		return true;
	}
	return false;
}
void caculateCacheSize(INT64 &csize,INT64 &cdsize,INT64 &hsize,INT64 &hdsize)
{
	csize = 0;
	cdsize = 0;
	hsize = 0;
	hdsize = 0;
	/*
	cacheLock.Lock();
	int i;
	for (i = 0; i < HASH_SIZE; i++) {
		hash_table[i].getSize(hsize, hdsize);
	}
	for (i=0;i<2;i++) {
		objList[i].getSize(csize,cdsize);
	}
	cacheLock.Unlock();
	*/
}
bool cache_prefetch(const char *url)
{
	kgl_async_http ctx;
	memset(&ctx,0,sizeof(ctx));
	ctx.meth = "GET";
	ctx.url = url;
	ctx.flags = KF_SIMULATE_LOCAL|KF_SIMULATE_CACHE|KF_SIMULATE_DELTA;
	KHttpHeader head = {(char *)"Accept-Encoding",(char *)"gzip",sizeof("Accept-Encoding")-1,sizeof("gzip")-1,NULL};
	ctx.rh = &head;
	return asyncHttpRequest(&ctx)==0;
}
