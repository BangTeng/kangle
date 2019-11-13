#include "KTempFile.h"
#include "KHttpRequest.h"
#include "kmalloc.h"
#include "kselector.h"
#include "http.h"
#include "directory.h"
#include "kforwin32.h"
#include "KHttpFilterStream.h"
#include "KHttpFilterManage.h"
#ifdef ENABLE_TF_EXCHANGE
#ifdef _WIN32
#define tunlink(f)             
#else
#define tunlink(f)             unlink(f)
#endif
//临时文件读post的处理器
kev_result resultTempFileReadPost(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	return rq->tf->readPostResult(rq,got);
}
int bufferTempFileReadPost(void *arg,LPWSABUF buf,int bufCount)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	int len;
	buf[0].iov_base = (char *)rq->tf->getPostBuffer(len);
	buf[0].iov_len = len;
	return 1;
}
KTempFile::KTempFile()
{
	total_size = 0;
	post_ctx = NULL;
}
KTempFile::~KTempFile()
{
	if (fp.opened()) {
		tunlink(file.c_str());
	}
	if (post_ctx) {
		delete post_ctx;
	}
}
void KTempFile::init(INT64 length)
{
	total_size = 0;
	this->length = length;
	if (fp.opened()) {
		fp.close();
		tunlink(file.c_str());
	}
	buffer.destroy();
}
int KTempFile::write(const char *buf, int len)
{
	int size;
	char *t = buffer.getWriteBuffer(size);
	if (t == NULL || size == 0) {
		return -1;
	}
	size = MIN(len, size);
	kassert(size > 0);
	memcpy(t, buf, size);
	buf += size;
	len -= size;
	if (!writeSuccess(size)) {
		return -1;
	}
	return size;
}
char *KTempFile::writeBuffer(int &size)
{
	char *t = buffer.getWriteBuffer(size);
	if (t && !IsLengthUnknow()) {
		INT64 left = length - total_size;
		size = (int)MIN((INT64)size,left);
	}
	return t;
}
char *KTempFile::getPostBuffer(int &size)
{
	if (HasPostStream()) {
		if (IsLengthUnknow()) {
			size = TEMPFILE_POST_CHUNK_SIZE;
		} else {
			INT64 left = length - total_size;
			size = (int)MIN(left, TEMPFILE_POST_CHUNK_SIZE);
		}
		return post_ctx->buffer;
	}
	return writeBuffer(size);
}
bool KTempFile::writeSuccess(int got)
{
	buffer.writeSuccess(got);
	total_size += got;
	if (!HasPostStream() && !IsLengthUnknow()) {
		if (total_size >= length) {
			return false;
		}
	}
	if (buffer.getLen() < 16384) {
		return true;
	}
	if (!openFile()) {
		return false;
	}
	return dumpOutBuffer();
}
bool KTempFile::dumpOutBuffer()
{
	kbuf *head = buffer.getHead();
	while (head) {
		if (head->used>0) {
			if ((int)head->used!=(int)fp.write(head->data,head->used)) {
				return false;
			}
		}
		head = head->next;
	}
	buffer.destroy();
	return true;
}
bool KTempFile::dumpInBuffer()
{
	kassert(buffer.getHead()==NULL);
	while (buffer.getLen()<16384) {
		char *data = (char *)malloc(NBUFF_SIZE);
		int used = fp.read(data, NBUFF_SIZE);
		if (used<=0) {
			free(data);
			break;
		}
		kbuf *buf = (kbuf *)malloc(sizeof(kbuf));
		buf->data = data;
		buf->used = used;
		buffer.Append(buf);
	}
	buffer.startRead();
	return buffer.getLen()>0;
}
bool KTempFile::openFile()
{
	if (fp.opened()) {
		return true;
	}
	std::stringstream s;
	s << conf.tmppath << "krf" << m_ppid << "_" << m_pid << "_" << (void *)this;
	s.str().swap(file);
	return fp.open(file.c_str(),fileWriteRead,KFILE_TEMP_MODEL);
}
INT64 KTempFile::switchRead()
{	
	if (fp.opened()) {
		dumpOutBuffer();
		fp.seek(0,seekBegin);
		dumpInBuffer();
	} else {
		buffer.startRead();
	}
	return total_size;
}
char *KTempFile::readBuffer(int &size)
{
	return buffer.getReadBuffer(size);
}
bool KTempFile::readSuccess(int got)
{
	bool result = buffer.readSuccess(got);
	if (result) {
		return true;
	}
	if (!fp.opened()) {
		return false;
	}
	buffer.destroy();
	return dumpInBuffer();
}
void KTempFile::resetRead()
{
	if (fp.opened()) {
		buffer.destroy();
		fp.seek(0,seekBegin);
		dumpInBuffer();
		return;
	}
	buffer.startRead();
}
int KTempFile::readBuffer(char *buf,int size)
{
	int len;
	char *t = buffer.getReadBuffer(len);
	if (t) {
		size = MIN(size,len);
		memcpy(buf,t,size);
		buffer.readSuccess(size);
		return size;
	}
	if (fp.opened()) {
		size = fp.read(buf,size);
		return size;
	}
	return 0;
}
StreamState KTempFile::writePostSuccess(int got)
{
	if (!HasPostStream()) {
		if (!writeSuccess(got)) {
			return STREAM_WRITE_END;
		}
		return STREAM_WRITE_SUCCESS;
	}
	total_size += got;
	StreamState ret = post_ctx->post_stream->write_all(post_ctx->buffer,got);
	if (ret == STREAM_WRITE_SUCCESS && !IsLengthUnknow() &&	total_size>=length) {
		ret = post_ctx->post_stream->write_end();
		if (ret!=STREAM_WRITE_SUCCESS) {
			return ret;
		}
		return STREAM_WRITE_END;
	}
	return ret;
}
/* 返回true，表示可能还有数据，还要再执行,false表示已经处理了 */
kev_result KTempFile::readPostResult(KHttpRequest *rq,int got)
{
	if (got<0 || (got==0 && !IsLengthUnknow())) {
		//read post error.
		SET(rq->flags,RQ_CONNECTION_CLOSE);
		return stageTempFileWriteEnd(rq);
	}
	if (got == 0) {
		this->length = 0;
	}
	int buf_size;
	char *buf = getPostBuffer(buf_size);
#ifdef ENABLE_INPUT_FILTER
	if (rq->if_ctx && JUMP_DENY==rq->if_ctx->check(buf,got,checkLast(got))) {
		return denyInputFilter(rq);
	}
#endif
	switch (writePostSuccess(got)) {
	case STREAM_WRITE_END:
		return read_post_end(rq);
	case STREAM_WRITE_SUCCESS:
		if (length==0) {
			return read_post_end(rq);
		}
		return rq->LowRead(rq,resultTempFileReadPost,bufferTempFileReadPost);
	default:
		return stageEndRequest(rq);
	}
}
kev_result KTempFile::read_post_end(KHttpRequest *rq)
{
	switchRead();
	if (rq->content_length!= total_size) {
		adjustPostContentLength(rq);
	}
	if (post_ctx && post_ctx->handle) {
		AfterPostHandle handle = post_ctx->handle;
		void *arg = post_ctx->arg;
		//提前清理post上下文，尽可能节省内存。
		delete post_ctx;
		post_ctx = NULL;
		return handle(rq,arg);
	}
	return processQueueRequest(rq);
}
kev_result KTempFile::startPost(KHttpRequest *rq,AfterPostHandle handle,void *arg)
{
	if (post_ctx==NULL) {
		post_ctx = new KTempFilePostContext;
	}
	post_ctx->handle = handle;
	post_ctx->arg = arg;
	if (length==0) {
		return read_post_end(rq);
	}
	return rq->LowRead(rq,resultTempFileReadPost,bufferTempFileReadPost);
}
void KTempFile::adjustPostContentLength(KHttpRequest *rq)
{
	rq->content_length = total_size;
	rq->left_read = total_size;
	if (TEST(rq->flags,RQ_INPUT_CHUNKED)) {
		CLR(rq->flags,RQ_INPUT_CHUNKED);
	}
	SET(rq->flags, RQ_HAS_CONTENT_LEN);
}
kev_result stageTempFileReadPost(KHttpRequest *rq,AfterPostHandle handle,void *arg)
{
	return rq->tf->startPost(rq,handle,arg);
}
int listHandleTempFile(const char *file,void *param)
{
	if (filencmp(file,"krf",3)!=0) {
		return 0;
	}
	int pid = atoi(file+3);
	if (pid == m_ppid) {
		return 0;
	}
	std::stringstream s;
	s << conf.tmppath << file;
	klog(KLOG_NOTICE,"remove uncleaned tmpfile [%s]\n",s.str().c_str());
	unlink(s.str().c_str());
	return 0;
}
KTHREAD_FUNCTION clean_tempfile_thread(void *param)
{
	klog(KLOG_DEBUG,"start to clean tmp file thread...\n");
	list_dir(conf.tmppath.c_str(),listHandleTempFile,NULL);
	klog(KLOG_DEBUG,"clean tmp file done.\n");
	KTHREAD_RETURN;
}
#endif
