#include "KStaticFetchObject.h"
#include "http.h"
#include "KContentType.h"
#include "KVirtualHostManage.h"

void bufferStaticAsyncRead(void *arg,iovec *buf,int &bufCount)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KStaticFetchObject *fo = static_cast<KStaticFetchObject *>(rq->fetchObj);
	fo->getAsyncBuffer(rq,buf,bufCount);
}
kev_result resultStaticAsyncRead(kasync_file *fp, void *arg, char *buf, int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KStaticFetchObject *fo = static_cast<KStaticFetchObject *>(rq->fetchObj);
	return fo->handleAsyncReadBody(rq,buf,got);
}

KTHREAD_FUNCTION simulateAsyncRead(void *param,int msec)
{
	KHttpRequest *rq = (KHttpRequest *)param;
	KStaticFetchObject *fo = static_cast<KStaticFetchObject *>(rq->fetchObj);
	fo->syncReadBody(rq);
	KTHREAD_RETURN;
}
kev_result KStaticFetchObject::open(KHttpRequest *rq)
{
	KFetchObject::open(rq);
	kassert(!rq->file->isDirectory());
	KHttpObject *obj = rq->ctx->obj;
	SET(obj->index.flags,ANSW_HAS_CONTENT_LENGTH|ANSW_LOCAL_SERVER|FLAG_NO_DISK_CACHE);
	if (rq->ctx->lastModified > 0 && rq->ctx->lastModified == rq->file->getLastModified()) {
		if (rq->ctx->mt==modified_if_modified) {
			obj->data->status_code = STATUS_NOT_MODIFIED;
			return handleUpstreamRecvedHead(rq);
		}
	} else if (rq->ctx->mt == modified_if_range_date||rq->ctx->mt == modified_if_range_etag) {
		CLR(rq->flags,RQ_HAVE_RANGE);
	}
	bool async_aio = true;
	assert(rq->file);
	FILE_HANDLE fp;
	kfinit(fp);
#ifdef ENABLE_UNICODE_FILE
	const wchar_t *wfilename = rq->file->getNameW();
	if (wfilename) {
		fp = kfopen_w(wfilename,fileRead,(async_aio ?KFILE_ASYNC:0));
	}
#else
	const char *filename = rq->file->getName();
	if (filename) {
		fp = kfopen(filename,fileRead,(async_aio ?KFILE_ASYNC:0));
	}
#endif
	if (!kflike(fp)) {
		return handleError(rq,STATUS_NOT_FOUND,"file not found");
	}
	if (async_aio) {
		//初始化异步相关数据
		kasync_file *aio_file = kgl_selector_module.aio_open(rq->sink->GetSelector(),fp);
		if (aio_file) {
			ad = new KAsyncData(aio_file, conf.io_buffer);
		}
	}
	if (ad==NULL) {
		this->fp = fp;
	}

#ifdef ENABLE_TF_EXCHANGE
	if (rq->tf) {
		//静态下载无需临时文件
		delete rq->tf;
		rq->tf = NULL;
	}
#endif
	//处理content-type
	if (!stageContentType(rq,obj)) {
		return handleError(rq,STATUS_FORBIDEN,"cann't find such content-type");
	}
	//设置last-modified
	SET(obj->index.flags,ANSW_LAST_MODIFIED);
	obj->index.content_length = rq->file->fileSize;
	obj->index.last_modified = rq->file->getLastModified();
	char tmp_buf[42];
	mk1123time(obj->index.last_modified, tmp_buf, 41);
	obj->insertHttpHeader(kgl_expand_string("Last-Modified"),(const char *)tmp_buf,29);
	if (obj->need_gzip && TEST(rq->raw_url.encoding, KGL_ENCODING_GZIP) && rq->file->fileSize >= conf.min_gzip_length) {
		//如果可能压缩，则不回应206
		CLR(rq->flags, RQ_HAVE_RANGE);
	}
	if (TEST(rq->flags,RQ_HAVE_RANGE)) {
		//处理部分数据请求
		rq->ctx->content_range_length = rq->file->fileSize;
		INT64 content_length = rq->file->fileSize;
		if(!adjust_range(rq,rq->file->fileSize)){
			return handleError(rq,416,"range error");
		}
		if (ad) {
			ad->offset = rq->range_from;
		} else if (!kfseek(fp,rq->range_from,seekBegin)) {
			return handleError(rq,500,"cann't seek to right position");
		}
		if (!TEST(rq->raw_url.flags,KGL_URL_RANGED)) {
			KStringBuf b;
			char buf[INT2STRING_LEN];
			b.WSTR("bytes ");
			b << int2string(rq->range_from, buf) << "-" ;
			b << int2string(rq->range_to, buf) << "/" ;
			b << int2string(content_length, buf);
			int len = b.getSize();
			obj->insertHttpHeader2(xstrdup("Content-Range"),sizeof("Content-Range")-1,b.stealString(),len);			
			obj->data->status_code = STATUS_CONTENT_PARTIAL;
			SET(obj->index.flags,ANSW_HAS_CONTENT_RANGE);
		} else {
			//url range的本地不缓存
			SET(obj->index.flags,ANSW_NO_CACHE);
		}
		obj->index.content_length = rq->file->fileSize;
	}
	//设置status_code
	if (obj->data->status_code==0) {
		obj->data->status_code = STATUS_OK;
	}
	//rq->buffer << "1234";
	//通知http头已经处理完成
	return handleUpstreamRecvedHead(rq);
}
kev_result KStaticFetchObject::readBody(KHttpRequest *rq)
{
	assert(rq->file);	
	if (ad) {
		return asyncReadBody(rq);
	}
	return syncReadBody(rq);
}
kev_result KStaticFetchObject::syncReadBody(KHttpRequest *rq)
{
	assert(rq->tr);
	char buf[8192];
	int len;
	do {
		len = (int) MIN(rq->file->fileSize,(INT64)sizeof(buf));
		if (len <= 0) {
			return stage_rdata_end(rq,STREAM_WRITE_SUCCESS);
		}	
		len = kfread(fp,buf, len);
		if(len<=0){
			return stage_rdata_end(rq,STREAM_WRITE_FAILED);
		}	
		rq->file->fileSize -= len;
	} while(!KEV_HANDLED(pushHttpBody(rq,buf,len)));

	return kev_ok;
}
//异步io读结果
kev_result KStaticFetchObject::handleAsyncReadBody(KHttpRequest *rq,char *buf,int got)
{
	if (got<=0) {
		return stage_rdata_end(rq,STREAM_WRITE_FAILED);
	}
	rq->file->fileSize -= got;
	if (ad != NULL) {
		ad->offset += got;
	}
	kev_result ret = pushHttpBody(rq, buf, got);
	if (KEV_HANDLED(ret)) {
		return ret;
	}
	return asyncReadBody(rq);
}
//异步io读
kev_result KStaticFetchObject::asyncReadBody(KHttpRequest *rq)
{
	int len = (int) MIN(rq->file->fileSize,(INT64)ad->buf_size);
	if (len <= 0) {
		return stage_rdata_end(rq,STREAM_WRITE_SUCCESS);
	}
	if (!kgl_selector_module.aio_read(rq->sink->GetSelector(), ad->aio_fp, ad->buf, ad->offset, len, resultStaticAsyncRead, rq)) {
		return stage_rdata_end(rq, STREAM_WRITE_FAILED);
	}
	return kev_ok;
}
