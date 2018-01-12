#include "KUwsgiFetchObject.h"
#include "http.h"

void KUwsgiFetchObject::buildHead(KHttpRequest *rq)
{
	buffer = new KSocketBuffer(NBUFF_SIZE);
	KHttpObject *obj = rq->ctx->obj;
	SET(obj->index.flags,ANSW_LOCAL_SERVER);
	bool chrooted = false;
#ifndef _WIN32
	chrooted = rq->svh->vh->chroot;
#endif
	make_http_env(rq,brd, rq->ctx->lastModified, rq->file,this, chrooted);
	int len = buffer->getLen();
	uwsgi_packet_header *header = buffer->insert<uwsgi_packet_header>();
	header->modifier1 = 0;
	header->datasize = (u_short)len;
	header->modifier2 = 0;
	hook.init(rq->ctx->obj,rq);
}
Parse_Result KUwsgiFetchObject::parseHead(KHttpRequest *rq,char *data,int len)
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

bool KUwsgiFetchObject::addEnv(const char *attr, const char *val)
{
	//write attr
	u_short len = (u_short)strlen(attr);
	buffer->write_all((char *)&len,sizeof(len));
	buffer->write_all(attr,len);
	//write val
	len = (u_short)strlen(val);
	buffer->write_all((char *)&len,sizeof(len));
	buffer->write_all(val,len);
	return true;
}
bool KUwsgiFetchObject::addHttpHeader(char *attr, char *val)
{
	u_short len = (u_short)strlen(attr) + 5;
	buffer->write_all((char *)&len,sizeof(len));
	buffer->write_all("HTTP_",5);
	char *hot = attr;
	while (*hot) {
		if (*hot == '-') {
			*hot = '_';
		} else {
			*hot = toupper(*hot);
		}
		hot++;
	}
	buffer->write_all(attr,len-5);
	//write val
	len = (u_short)strlen(val) ;
	buffer->write_all((char *)&len,sizeof(len));
	buffer->write_all(val,len);
	return true;
}
