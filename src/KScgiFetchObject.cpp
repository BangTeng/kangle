#include "http.h"
#include "KScgiFetchObject.h"
void KScgiFetchObject::buildHead(KHttpRequest *rq)
{
	buffer = new KSocketBuffer(NBUFF_SIZE);
	KHttpObject *obj = rq->ctx->obj;
	SET(obj->index.flags,ANSW_LOCAL_SERVER);
	bool chrooted = false;
#ifndef _WIN32
	chrooted = rq->svh->vh->chroot;
#endif
	make_http_env(rq,brd, rq->ctx->lastModified, rq->file, this, chrooted);
	addEnv("SCGI","1");
	if (rq->content_length==0) {
		addEnv("CONTENT_LENGTH","0");
	}
	int len = buffer->getLen();
	buff *buf = (buff *)malloc(sizeof(buff));
	buf->data = (char *)malloc(16);
	len = snprintf(buf->data,16,"%d:",len);
	buf->used = len;
	buffer->insertBuffer(buf);
	buffer->write_all(",",1);
	hook.init(rq->ctx->obj,rq);
	hook.setProto(Proto_scgi);
}
Parse_Result KScgiFetchObject::parseHead(KHttpRequest *rq,char *data,int len)
{
	assert(header && hot);
	switch(parser.parse(header,hot-header,&hook)){
		case HTTP_PARSE_FAILED:
			//ÖØÖÃhot
			hot = NULL;
			return Parse_Failed;
		case HTTP_PARSE_SUCCESS:
			rq->ctx->obj->data->headers = parser.stealHeaders(rq->ctx->obj->data->headers);
			//ÖØÖÃhot
			hot = NULL;
			return Parse_Success;
	}
	return Parse_Continue;
}
bool KScgiFetchObject::addEnv(const char *attr, const char *val)
{
	if (strcmp(attr,"CONTENT_LENGTH")==0) {
		content_length_added = true;
	}
	int len = strlen(attr);
	buffer->write_all(attr,len+1);
	len = strlen(val);
	buffer->write_all(val,len+1);
	return true;
}
bool KScgiFetchObject::addHttpHeader(char *attr, char *val)
{
	char *hot = attr;
	while (*hot) {
		if (*hot == '-') {
			*hot = '_';
		} else {
			*hot = toupper(*hot);
		}
		hot++;
	}
	if(strcmp(attr,"CONTENT_LENGTH")==0 || strcmp(attr,"CONTENT_TYPE")==0){
		return addEnv(attr,val);
	}
	int len = strlen(attr);
	buffer->write_all("HTTP_",5);
	buffer->write_all(attr,len+1);
	len = strlen(val);
	buffer->write_all(val,len+1);
	return true;
}
