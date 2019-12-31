#include "KDiskCacheStream.h"
#include "KHttpRequest.h"
#include "KHttpObject.h"
#ifdef ENABLE_DISK_CACHE
kev_result flush_disk_cache(KHttpRequest *rq);
kev_result flush_disk_cache_callback(kasync_file *fp, void *arg, char *buf, int length)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KDiskCacheContext *ctx = (KDiskCacheContext *)rq->GetCurrentWriteHookContext();
	if (length <= 0) {
		SET(rq->ctx->obj->index.flags, FLAG_DEAD);
		delete ctx;
		return rq->WriteHookCallBack();
	}
	ctx->hot += length;
	ctx->offset += length;
	ctx->size -= length;
	return flush_disk_cache(rq);
}
kev_result flush_disk_cache(KHttpRequest *rq)
{
	KDiskCacheContext *ctx = (KDiskCacheContext *)rq->GetCurrentWriteHookContext();
	if (ctx->size > 0) {
		kasync_file *file = ctx->disk_cache->GetFile();
		kassert(file != NULL);
		if (file == NULL) {
			klog(KLOG_ERR, "BUG!! flush_disk_cache is NULL.\n");
			delete ctx;
			return rq->WriteHookCallBack();
		}
		if (!kgl_selector_module.aio_write(rq->sink->GetSelector(), file, ctx->hot, ctx->offset, ctx->size, flush_disk_cache_callback, rq)) {
			return flush_disk_cache_callback(file, rq, NULL, -1);
		}
		return kev_ok;
	}
	delete ctx;
	return rq->WriteHookCallBack();
}
kev_result close_disk_cache(KHttpRequest *rq)
{
	KDiskCacheStream *ctx = (KDiskCacheStream *)rq->GetCurrentWriteHookContext();
	ctx->RealClose();
	return rq->WriteHookCallBack();
}
bool KDiskCacheStream::Open(KHttpRequest *rq,KHttpObject *obj)
{
	filename = obj->getFileName();
	fileModel model = fileWrite;
	FILE_HANDLE fp = kfopen(filename, model, KFILE_ASYNC);
	if (!kflike(fp)) {
		return false;
	}
	kassert(aio_file == NULL);
	aio_file = kgl_selector_module.aio_open(rq->sink->GetSelector(), fp);
	if (aio_file == NULL) {
		kfclose(fp);
		return false;
	}
	obj->caculate_header_size(0);
	return true;
}
bool KDiskCacheStream::Write(KHttpRequest *rq, KHttpObject *obj, const char *buf, int len)
{
	while (len > 0) {
		if (buffer == NULL) {
			buffer_left = conf.io_buffer;
			buffer = (char *)aio_alloc_buffer(buffer_left);
			hot = buffer;
		}
		int this_len = MIN(buffer_left, len);
		memcpy(hot, buf, this_len);
		hot += this_len;
		buf += this_len;
		buffer_left -= this_len;
		len -= this_len;
		if (buffer_left <= 0) {
			FlushBuffer(rq,obj->index.head_size);
		}
	}
	return true;
}
void KDiskCacheStream::RealClose()
{
	kasync_file_close(aio_file);
	aio_file = NULL;
	kassert(buffer == NULL);
}
bool KDiskCacheStream::Close(KHttpRequest *rq, KHttpObject *obj)
{
	if (buffer) {
		FlushBuffer(rq, obj->index.head_size);
	}
	//write meta info
	KDiskCacheContext *ctx = new KDiskCacheContext();
	ctx->buffer = obj->build_aio_header(ctx->size);
	ctx->offset = 0;
	ctx->hot = ctx->buffer;
	ctx->disk_cache = this;
	rq->AddWriteHook(ctx, flush_disk_cache,false);
	//close
	rq->AddWriteHook(this, close_disk_cache, true);
	return true;
}
void KDiskCacheStream::FlushBuffer(KHttpRequest *rq,int head_size)
{
	KDiskCacheContext *ctx = new KDiskCacheContext();
	ctx->buffer = buffer;
	ctx->hot = ctx->buffer;
	ctx->offset = this->offset + head_size;
	ctx->disk_cache = this;
	ctx->size = hot - buffer;
	//更新下一个offset
	this->offset += ctx->size;
	rq->AddWriteHook(ctx, flush_disk_cache,false);
	hot = buffer = NULL;
}
#endif
