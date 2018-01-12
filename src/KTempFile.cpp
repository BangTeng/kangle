#include "KTempFile.h"
#include "KHttpRequest.h"
#include "malloc_debug.h"
#include "KSelector.h"
#include "http.h"
#include "directory.h"
#include "forwin32.h"
#include "KHttpFilterStream.h"
#include "KHttpFilterManage.h"
#include "KTempFileStream.h"
#ifdef ENABLE_TF_EXCHANGE
#ifdef _WIN32
#define tunlink(f)             
#else
#define tunlink(f)             unlink(f)
#endif
//临时文件读post的处理器
void resultTempFileReadPost(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	rq->tf->readPostResult(rq,got);
}
void bufferTempFileReadPost(void *arg,LPWSABUF buf,int &bufCount)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	bufCount = 1;
	int len;
	buf[0].iov_base = (char *)rq->tf->getPostBuffer(len);
	buf[0].iov_len = len;
}
KTempFile::KTempFile()
{
	total_size = 0;
	writeModel = true;
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
	writeModel = true;
}
bool KTempFile::writeBuffer(KHttpRequest *rq,const char *buf,int len)
{
	while (len>0) {
		int size;
		char *t = buffer.getWBuffer(size);
		if (t==NULL || size==0) {
			return false;
		}
		size = MIN(len,size);
		kassert(size>0);
		memcpy(t,buf,size);
		buf += size;
		len -= size;
		if (!writeSuccess(rq,size)) {
			return false;
		}
	}
	return true;
}
char *KTempFile::writeBuffer(int &size)
{
	char *t = buffer.getWBuffer(size);
	if (t && length>0) {
		size = (int)MIN((INT64)size,(length - total_size));
	}
	return t;
}
char *KTempFile::getPostBuffer(int &size)
{
	if (post_ctx && post_ctx->post_stream) {
		INT64 left = length - total_size;
		size = (int)MIN(left,TEMPFILE_POST_CHUNK_SIZE);
		return post_ctx->buffer;
	} else {
		return writeBuffer(size);
	}
}
bool KTempFile::writeSuccess(KHttpRequest *rq,int got)
{
	buffer.writeSuccess(got);
	total_size += got;
	if ((post_ctx == NULL || post_ctx->post_stream==NULL) &&
		length > 0) {		
		if (total_size >= length) {
			return false;
		}
	}
	if (buffer.getLen() < 16384) {
		return true;
	}
	if (!openFile(rq)) {
		return false;
	}
	return dumpOutBuffer();
}
bool KTempFile::dumpOutBuffer()
{
	buff *head = buffer.getHead();
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
		buff *buf = (buff *)malloc(sizeof(buff));
		buf->data = data;
		buf->used = used;
		buffer.appendBuffer(buf);
	}
	buffer.startRead();
	return buffer.getLen()>0;
}
bool KTempFile::openFile(KHttpRequest *rq)
{
	if (fp.opened()) {
		return true;
	}
	std::stringstream s;
	s << conf.tmppath << "krf" << m_ppid << "_" << m_pid << "_" << (void *)rq;
	s.str().swap(file);
	return fp.open(file.c_str(),fileWriteRead,KFILE_TEMP_MODEL);
}
INT64 KTempFile::switchRead()
{	
	writeModel = false;
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
	return buffer.getRBuffer(size);
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
	} else {
		buffer.startRead();
	}
}
int KTempFile::readBuffer(char *buf,int size)
{
	int len;
	char *t = buffer.getRBuffer(len);
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
StreamState KTempFile::writePostSuccess(KHttpRequest *rq,int got)
{
	if (post_ctx==NULL || post_ctx->post_stream==NULL) {
		if (!writeSuccess(rq,got)) {
			return STREAM_WRITE_END;
		}
		return STREAM_WRITE_SUCCESS;
	}
	total_size += got;
	StreamState ret = post_ctx->post_stream->write_all(post_ctx->buffer,got);
	if (ret == STREAM_WRITE_SUCCESS &&
		length>0 &&
		total_size>=length) {
		ret = post_ctx->post_stream->write_end();
		if (ret!=STREAM_WRITE_SUCCESS) {
			return ret;
		}
		return STREAM_WRITE_END;
	}
	return ret;
}
/* 返回true，表示可能还有数据，还要再执行,false表示已经处理了 */
bool KTempFile::readPostResult(KHttpRequest *rq,int got)
{
	int buf_size;
	char *buf = getPostBuffer(buf_size);
	if (got<=0) {
		SET(rq->flags,RQ_CONNECTION_CLOSE);
		stageTempFileWriteEnd(rq);
		return false;
	}
	//fwrite(buf,1,got,stdout);
#ifdef ENABLE_INPUT_FILTER
	if (rq->if_ctx) {
		if (rq->if_ctx->dechunk) {
			const char *hot_buf = buf;
			const char *piece = NULL;
			int got2 = 0;
			char *dechunk_buf = buf;
			int piece_len = 0;
			//printf("dechunk************\n");
			while (got>0) {
				dechunk_status de_status = rq->if_ctx->dechunk->dechunk(&hot_buf,got,&piece,piece_len);
				if (de_status==dechunk_failed) {
					SET(rq->flags,RQ_CONNECTION_CLOSE);
					stageTempFileWriteEnd(rq);
					return false;
				}
				if (de_status==dechunk_end) {
					this->length = 0;
					break;
				}
				if (piece) {
					memcpy(dechunk_buf,piece,piece_len);
					dechunk_buf += piece_len;
					fwrite(piece,1,piece_len,stdout);
					got2 += piece_len;
				}
				if (de_status==dechunk_continue) {
					break;
				}
			}
			got = got2;
		}
		if (JUMP_DENY==rq->if_ctx->check(buf,got,checkLast(got))) {
			denyInputFilter(rq);
			return false;
		}
	}
#endif
	switch (writePostSuccess(rq,got)) {
	case STREAM_WRITE_END:
		read_post_end(rq);
		break;
	case STREAM_WRITE_SUCCESS:
		if (length==0) {
			read_post_end(rq);
			break;
		}
		if (try_pre_load_body(rq)) {
			break;
		}
		rq->c->read(rq,resultTempFileReadPost,bufferTempFileReadPost);
		break;
	default:
		stageEndRequest(rq);
		break;
	}
	return false;
}
void KTempFile::read_post_end(KHttpRequest *rq)
{
	switchRead();
	if (TEST(rq->flags,RQ_INPUT_CHUNKED)|| (post_ctx && post_ctx->post_stream)) {
		adjustPostContentLength(rq);
	}
	if (post_ctx && post_ctx->handle) {
		AfterPostHandle handle = post_ctx->handle;
		void *arg = post_ctx->arg;
		//提前清理post上下文，尽可能节省内存。
		delete post_ctx;
		post_ctx = NULL;
		handle(rq,arg);
	} else {
		processQueueRequest(rq);
	}
}
void KTempFile::startPost(KHttpRequest *rq,AfterPostHandle handle,void *arg)
{
	KHttpStream *head = NULL;
	KHttpStream *end = NULL;
#ifdef ENABLE_KSAPI_FILTER
	KHttpFilterManage::buildReadStream(rq,&head,&end);
	if (head==NULL) {
		assert(end==NULL);
#endif
		if (rq->fetchObj && !rq->fetchObj->needTempFile()) {
			//无post数据过滤，上游又不用临时文件
			rq->closeTempFile();
			handle(rq,arg);
			return;
		}
#ifdef ENABLE_KSAPI_FILTER
	}
#endif
	if (post_ctx==NULL) {
		post_ctx = new KTempFilePostContext;
	}
	post_ctx->handle = handle;
	post_ctx->arg = arg;
	if (head) {
		assert(end);
		post_ctx->buffer = (char *)xmalloc(TEMPFILE_POST_CHUNK_SIZE);
		post_ctx->post_stream = head;
		KTempFileStream *tf_stream = new KTempFileStream(rq);
		end->connect(tf_stream,true);
	}
	if (try_pre_load_body(rq)) {
		return;
	}
	if (length==0) {
		read_post_end(rq);
		return;
	}
	rq->c->read(rq,resultTempFileReadPost,bufferTempFileReadPost);
	/*
	if (rq->pre_post_length>0) {
		int buf_size;
		char *buf = getPostBuffer(buf_size);

		StreamState result = post_ctx->post_stream->write_all(rq->parser.body,rq->pre_post_length);
		rq->parser.bodyLen -= rq->pre_post_length;
		rq->parser.body += rq->pre_post_length;
		rq->pre_post_length = 0;
		switch (result) {
		case STREAM_WRITE_END:
		case STREAM_WRITE_FAILED:
			stageEndRequest(rq);
			return;
		case STREAM_WRITE_HANDLED:
			return;
		default:
			break;
		}
	}
	if (length<=0) {
		//没有了post数据要读
		if (post_ctx->post_stream) {
			StreamState result = post_ctx->post_stream->write_end();
			switch (result) {
			case STREAM_WRITE_END:
			case STREAM_WRITE_FAILED:
				stageEndRequest(rq);
				return;
			case STREAM_WRITE_HANDLED:
				return;
			default:
				break;
			}
		}
		switchRead();
		adjustPostContentLength(rq);
		handle(rq,arg);
		return;
	}
	rq->c->read(rq,resultTempFileReadPost,bufferTempFileReadPost);
	return;
	*/
}
bool KTempFile::try_pre_load_body(KHttpRequest *rq)
{
	if (rq->pre_post_length>0) {
		int buf_size;
		char *buf = getPostBuffer(buf_size);
		int copy_size = MIN(buf_size,rq->pre_post_length);
		memcpy(buf,rq->parser.body,copy_size);
		rq->parser.bodyLen -= copy_size;
		rq->parser.body += copy_size;
		rq->pre_post_length -= copy_size;
		readPostResult(rq,copy_size);
		return true;
	}
	return false;
}
void KTempFile::adjustPostContentLength(KHttpRequest *rq)
{
	rq->content_length = total_size;
	rq->left_read = total_size;
	if (TEST(rq->flags,RQ_INPUT_CHUNKED)) {
		CLR(rq->flags,RQ_INPUT_CHUNKED);
		SET(rq->flags,RQ_HAS_CONTENT_LEN);
	}
}
void stageTempFileReadPost(KHttpRequest *rq,AfterPostHandle handle,void *arg)
{
	rq->tf->startPost(rq,handle,arg);
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
FUNC_TYPE FUNC_CALL clean_tempfile_thread(void *param)
{
	klog(KLOG_DEBUG,"start to clean tmp file thread...\n");
	list_dir(conf.tmppath.c_str(),listHandleTempFile,NULL);
	klog(KLOG_DEBUG,"clean tmp file done.\n");
	KTHREAD_RETURN;
}
#endif
