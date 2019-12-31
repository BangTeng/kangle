#include "KWriteBack.h"
#include "KHttpObject.h"
#include "KBufferFetchObject.h"
#include "kmalloc.h"
using namespace std;

std::string KWriteBack::getMsg()
{
	std::stringstream s;
	kgl_str_t ret;
	getRequestLine(status_code,&ret);
	s.write(ret.data, ret.len);
	KHttpHeader *h = header;
	while (h) {
		s << h->attr << ": " << h->val << "\r\n";
		h = h->next;
	}
	if (keep_alive) {
		s << "Connection: keep-alive\r\n";
	}
	s << "\r\n";
	if (body.getSize()) {
		s << body.getString();
	}
	return s.str();
}
void KWriteBack::setMsg(std::string msg)
{
	if (header) {
		free_header(header);
		header = NULL;
	}
	body.clean();
	if (msg.empty()) {
		return;
	}
	char *buf = strdup(msg.c_str());
	KWriteBackParser parser;
	int len = (int)msg.size();
	char *data = buf;
	parser.Parse(&data,&len);
	status_code = parser.status_code;
	keep_alive = parser.keep_alive;
	header = parser.StealHeader();
	if (len>0) {
		body.write_all(data,len);
	}
	xfree(buf);
}
void KWriteBack::buildRequest(KHttpRequest *rq)
{
	rq->responseStatus(status_code);
	KHttpHeader *h = header;
	while (h) {
		rq->responseHeader(h->attr,h->attr_len,h->val,h->val_len);
		h = h->next;
	}
	rq->responseHeader(kgl_expand_string("Content-Length"),body.getSize());
	if (!keep_alive) {
		SET(rq->flags,RQ_CONNECTION_CLOSE);
	}
	rq->responseConnection();
	if (rq->meth!=METH_HEAD) {
		KAutoBuffer buffer(rq->pool);
		buffer.write_all(body.getBuf(),body.getSize());		
		rq->appendFetchObject(new KBufferFetchObject(buffer.getHead(), 0, buffer.getLen(), NULL));
	}
}
