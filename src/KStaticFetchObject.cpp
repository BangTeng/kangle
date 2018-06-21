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
void resultStaticAsyncRead(KAsyncFile *fp, void *arg, char *buf, int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KStaticFetchObject *fo = static_cast<KStaticFetchObject *>(rq->fetchObj);
	fo->handleAsyncReadBody(rq,buf,got);
}

KTHREAD_FUNCTION simulateAsyncRead(void *param,int msec)
{
	KHttpRequest *rq = (KHttpRequest *)param;
	KStaticFetchObject *fo = static_cast<KStaticFetchObject *>(rq->fetchObj);
	fo->syncReadBody(rq);
	KTHREAD_RETURN;
}
void KStaticFetchObject::open(KHttpRequest *rq)
{
	KFetchObject::open(rq);
	assert(!rq->file->isDirectory());
	KHttpObject *obj = rq->ctx->obj;
	SET(obj->index.flags,ANSW_HAS_CONTENT_LENGTH|ANSW_LOCAL_SERVER);
	if (rq->ctx->lastModified > 0 && rq->ctx->lastModified == rq->file->getLastModified()) {
		if (rq->ctx->mt==modified_if_modified) {
			obj->data->status_code = STATUS_NOT_MODIFIED;
			handleUpstreamRecvedHead(rq);
			return;
		}
	} else if (rq->ctx->mt == modified_if_range_date||rq->ctx->mt == modified_if_range_etag) {
		CLR(rq->flags,RQ_HAVE_RANGE);
	}
	assert(rq->file);
	bool async_aio = true;
#ifdef _WIN32
	
#else
	const char *filename = rq->file->getName();
	if (filename) {
		fp.open(filename,fileRead,(async_aio ?KFILE_ASYNC:0));
	}
#endif
	if (!fp.opened()) {
		handleError(rq,STATUS_NOT_FOUND,"file not found");
		return;
	}
	if (async_aio) {
		//初始化异步相关数据
		KAsyncFile *aio_file = rq->c->selector->aio_open(&fp);
		if (aio_file) {
			ad = new KAsyncData(aio_file, conf.io_buffer);
		}
	}
	if (!rq->sr) {
#ifdef ENABLE_TF_EXCHANGE
		if (rq->tf) {
			//静态下载无需临时文件
			delete rq->tf;
			rq->tf = NULL;
		}
#endif
		//处理content-type
		if (!stageContentType(rq,obj)) {
			handleError(rq,STATUS_FORBIDEN,"cann't find such content-type");
			return;
		}
		//设置last-modified
		SET(obj->index.flags,ANSW_LAST_MODIFIED);
		obj->index.content_length = rq->file->fileSize;
		obj->index.last_modified = rq->file->getLastModified();
		char tmp_buf[42];
		mk1123time(obj->index.last_modified, tmp_buf, 41);
		obj->insertHttpHeader(kgl_expand_string("Last-Modified"),(const char *)tmp_buf,29);
	}
	if (TEST(rq->flags,RQ_HAVE_RANGE)) {
		//处理部分数据请求		
		rq->ctx->content_range_length = rq->file->fileSize;
		INT64 content_length = rq->file->fileSize;
		if(!adjust_range(rq,rq->file->fileSize)){
			handleError(rq,416,"range error");
		    return;
		}
		if (ad) {
			ad->offset = rq->range_from;
		} else if (!fp.seek(rq->range_from,seekBegin)) {
			handleError(rq,500,"cann't seek to right position");
			return ;
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
	handleUpstreamRecvedHead(rq);
}
void KStaticFetchObject::readBody(KHttpRequest *rq)
{
	assert(rq->file);
	assert(fp.opened());
	if (ad) {
		asyncReadBody(rq);
		return;
	}
	syncReadBody(rq);
}
void KStaticFetchObject::syncReadBody(KHttpRequest *rq)
{
	assert(rq->ctx->st);
	//if (rq->ctx->st->support_sendfile()) {
		//todo: sendfile;
		//printf("support sendfile\n");
	//}
	char buf[8192];
	int len;
	do {
		len = (int) MIN(rq->file->fileSize,(INT64)sizeof(buf));
		if (len <= 0) {
			stage_rdata_end(rq,STREAM_WRITE_SUCCESS);
			return ;
		}	
		len = fp.read(buf, len);
		if(len<=0){
			stage_rdata_end(rq,STREAM_WRITE_FAILED);
			return ;
		}	
		rq->file->fileSize -= len;
	} while(pushHttpBody(rq,buf,len));
}
//异步io读结果
void KStaticFetchObject::handleAsyncReadBody(KHttpRequest *rq,char *buf,int got)
{
	if (got<=0) {
		stage_rdata_end(rq,STREAM_WRITE_FAILED);
		return;
	}
	rq->file->fileSize -= got;
	if (ad != NULL) {
		ad->offset += got;
	}
	if (!pushHttpBody(rq,buf,got)) {
		return;
	}
	asyncReadBody(rq);
	return;
}
//异步io读
void KStaticFetchObject::asyncReadBody(KHttpRequest *rq)
{
	int len = (int) MIN(rq->file->fileSize,(INT64)ad->buf_size);
	if (len <= 0) {
		stage_rdata_end(rq,STREAM_WRITE_SUCCESS);
		return;
	}
	//rq->c->removeRequest(rq, true);
	if (!rq->c->selector->aio_read(ad->aio_fp, ad->buf, ad->offset, len, resultStaticAsyncRead, rq)) {
		stage_rdata_end(rq, STREAM_WRITE_FAILED);
		return;
	}
	return;
}
