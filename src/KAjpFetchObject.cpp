/*
 * KAjpFetchObject.cpp
 *
 *  Created on: 2010-7-31
 *      Author: keengo
 */
#include "http.h"
#include "KAjpFetchObject.h"
#include "KHttpTransfer.h"
#include "KHttpObjectParserHook.h"
#include "malloc_debug.h"
#define MAX_AJP_RESPONSE_HEADERS 0xc
#define MAX_AJP_REQUEST_HEADERS  0xf
#if 0 
static const char *ajp_request_headers[MAX_AJP_REQUEST_HEADERS] = {
	"",
	"accept",
	"accept-charset",
	"accept-encoding",
	"accept-language",
	"authorization",
	"connection",
	"content-type",
	"content-length",
	"cookie",
	"cookie2",
	"host",
	"pragma",
	"referer",
	"user-agent"
};
#endif
static const char *ajp_response_headers[MAX_AJP_RESPONSE_HEADERS]={
	"Unknow",
	"Content-Type",
	"Content-Language",
	"Content-Length",
	"Date",
	"Last-Modified",
	"Location",
	"Set-Cookie",
	"Set-Cookie2",
	"Servlet-Engine",
	"Status",
	"WWW-Authenticate"
};
KAjpFetchObject::KAjpFetchObject() {
	reuse = false;
	header_len = 0;
	body_len = -1;
	body = NULL;
	body_hot = body;
	parsed_len = 0;
	last_msg = NULL;
	bodyEnd = false;
}

KAjpFetchObject::~KAjpFetchObject() {
	if(body){
		xfree(body);
	}
	if(last_msg){
		delete last_msg;
	}
}
//创建发送头到buffer中。
void KAjpFetchObject::buildHead(KHttpRequest *rq)
{
	assert(buffer == NULL);
	buffer = new KSocketBuffer(AJP_BUFF_SIZE);
	char tmpbuff[50];
	KHttpObject *obj = rq->ctx->obj;
	SET(obj->index.flags,ANSW_LOCAL_SERVER);
	KAjpMessage b(buffer);
	b.putByte(JK_AJP13_FORWARD_REQUEST);
	b.putByte(rq->meth);
	b.putString("HTTP/1.1");
	if (TEST(rq->url->flags, KGL_URL_ENCODE)) {
		size_t path_len = 0;
		char *path = url_encode(rq->url->path, strlen(rq->url->path), &path_len);
		b.putString(path);
		free(path);
	} else {
		b.putString(rq->url->path);
	}
	b.putString(rq->getClientIp());
	//remote host
	b.putShort(0xffff);
	//b.putString(rq->c->socket->get_remote_ip());
	b.putString(rq->url->host);
	b.putShort(rq->c->socket->get_self_port());
	//is secure
	b.putByte(0);
	KHttpHeader *header = rq->parser.getHeaders();
	int count = 0;
	while(header) {
		if (!is_internal_header(header)) {
			count++;
		}
		header = header->next;
	}
	if(rq->ctx->lastModified>0){
		count++;
	}
	if (rq->content_length > 0) {
		count++;
	}
	b.putShort(count);
	//printf("head count=%d\n",count);
	header = rq->parser.getHeaders();
	bool founded;
	while(header){
		if (is_internal_header(header)) {
			goto do_not_insert;
		}
		//printf("try send attr[%s] val[%s]\n",header->attr,header->val);
		founded=false;
		/*
		for(unsigned short i=1;i<MAX_AJP_REQUEST_HEADERS;i++){
			if (strcasecmp(ajp_request_headers[i],header->attr)==0) {
				i |= 0xA000;
				b.putShort(i);
				founded=true;
				break;
			}
		}
		//*/
		if(!founded){
			b.putString(header->attr);
		}
		b.putString(header->val);
		do_not_insert:header = header->next;
	}
	if (rq->content_length > 0) {
		b.putShort(0xA008);
		b.putString((char *)int2string(rq->content_length, tmpbuff));
	}
	if (rq->ctx->lastModified > 0) {
		char mk1123buff[50];
		mk1123time(rq->ctx->lastModified, mk1123buff, sizeof(mk1123buff));
		b.putString("If-Modified-Since");
		b.putString(mk1123buff);
		//printf("send if-modified-since %s\n",mk1123buff);
	}
	if(rq->url->param){
		//printf("send query_string\n");
		b.putByte(0x05);
		b.putString(rq->url->param);
	}
	b.putByte(0xFF);
	b.end();
	while (rq->pre_post_length>0) {
		int len = MIN(rq->pre_post_length,NBUFF_SIZE);
		unsigned char h[6];
		h[0] = 0x12;h[1] = 0x34;
		int dlen = len + 2;
		h[2] = (dlen>>8 &0xFF);
		h[3] = (dlen & 0xFF);
		h[4] = (len>>8 & 0xFF);
		h[5] = (len & 0xFF);
		buffer->write_all((char *)h,6);
		buffer->write_all(rq->parser.body,len);
		rq->left_read -= len;
		rq->parser.bodyLen -= len;
		rq->parser.body += len;
		rq->pre_post_length -= len;
	}
	if (rq->left_read==0) {
		appendPostEnd();
	}
}
void KAjpFetchObject::appendPostEnd()
{
	buff *ebuff = (buff *)malloc(sizeof(buff));
	ebuff->data = (char *)malloc(4);
	ebuff->used = 4;
	char *d = ebuff->data;
	d[0] = 0x12;
	d[1] = 0x34;
	d[2] = 0;
	d[3] = 0;
	buffer->appendBuffer(ebuff);
}
//解析head
Parse_Result KAjpFetchObject::parseHead(KHttpRequest *rq,char *data,int len)
{
	//printf("parse head len=%d\n",len);
	//这里为什么不直接用data,len呢？而是要用header + parsed_len来确定？
	//因为有可能一个header里面包含多条ajpmessage，要保留解析位置，下次接着从上次位置解析。
	for (;;) {
		char *str = header + parsed_len;
		len = (int)(hot - header) - parsed_len;
		if (len<=0) {
			return Parse_Continue;
		}
		char *save_data = str;
		KAjpMessage *msg = parse(&str,len);
		int this_parsed_len = (int)(str - save_data);
		//printf("this_parsed_len=%d\n",this_parsed_len);
		parsed_len += this_parsed_len;
		if (msg == NULL) {
			return Parse_Continue;
		}
		unsigned char type = parseMessage(rq,msg);
		//printf("type=%d,len=%d\n",type,msg->getLen());
		delete msg;
		if (type==JK_AJP13_SEND_HEADERS) {
			return Parse_Success;
		}
		if (type==JK_AJP13_GET_BODY_CHUNK) {
			//tomcat期望body数据
			continue;
		}
		return Parse_Failed;
	}
	
}
unsigned char KAjpFetchObject::parseMessage(KHttpRequest *rq,KAjpMessage *msg)
{
	KHttpObject *obj = rq->ctx->obj;
	KHttpObjectParserHook hook(obj,rq);
	unsigned char type;
	if(!msg->getByte(&type)){
		return JK_AJP13_ERROR;
	}
	if(type == JK_AJP13_SEND_HEADERS){
		unsigned short status_code;
		if(!msg->getShort(&status_code)){
			return JK_AJP13_ERROR;
		}
		char *status_msg;
		if(!msg->getString(&status_msg)){
			return JK_AJP13_ERROR;
		}
		//printf("status=%d %s\n",status_code,status_msg);
		if(obj->data->status_code==0){
			obj->data->status_code = status_code;
		}
		unsigned short head_count;
		if(!msg->getShort(&head_count)){
			return JK_AJP13_ERROR;
		}
		//printf("head count=%d\n",head_count);
		unsigned char head_type;
		for(int i=0;i<head_count;i++){
			if(!msg->peekByte(&head_type)){
				return JK_AJP13_ERROR;
			}
			const char *attr = NULL;
			if(head_type==0xA0){
				unsigned short head_attr;
				if(!msg->getShort(&head_attr)){
					return JK_AJP13_ERROR;
				}
				head_attr = head_attr & 0xFF;
				if(head_attr>0 && head_attr<MAX_AJP_RESPONSE_HEADERS){
					attr = ajp_response_headers[head_attr];
				}

			}else{
				if(!msg->getString((char **)&attr)){
					return JK_AJP13_ERROR;
				}

			}
			char *val;
			if(!msg->getString(&val)){
				return JK_AJP13_ERROR;
			}
			//printf("attr=%s\n",attr);
			//printf("val=%s\n",val);
			if(attr){
				int val_len = strlen(val);
				if(hook.parseHeader(attr,val,val_len,false)==HTTP_PARSE_SUCCESS){
					obj->insertHttpHeader(attr,strlen(attr),val,val_len);
				}
			}
		}
	} else if(type == JK_AJP13_END_RESPONSE) {
		//printf("body end \n");
		bodyEnd = true;
		expectDone(rq);
	}
	return type;
}
//创建post数据到buffer中。
void KAjpFetchObject::buildPost(KHttpRequest *rq)
{
	unsigned len = buffer->getLen();
	//printf("buildpost len = %d\n",len);
	assert(len>0 && len<=AJP_PACKAGE);

	buff *nbuf = (buff *)malloc(sizeof(buff));
	nbuf->data = (char  *)malloc(6);
	nbuf->used = 6;
	unsigned char *h = (unsigned char *)nbuf->data;
	h[0] = 0x12;
	h[1] = 0x34;
	unsigned dlen = len + 2;
	h[2] = (dlen>>8 &0xFF);
	h[3] = (dlen & 0xFF);
	h[4] = (len>>8 & 0xFF);
	h[5] = (len & 0xFF);
	buffer->insertBuffer(nbuf);
	if (rq->left_read==0) {
		appendPostEnd();
	}
}
//读取body数据
char *KAjpFetchObject::nextBody(KHttpRequest *rq,int &len)
{
	if(last_msg){
		delete last_msg;
		last_msg = NULL;
	}
	char *str = header + parsed_len;
	len = hot - header - parsed_len;
	//printf("next Body len = %d\n",len);
	if (len==0) {
		return NULL;
	}
	last_msg = parse(&str,len);
	parsed_len = str - header;
	if(last_msg == NULL){
		return NULL;
	}
	unsigned char type = parseMessage(rq,last_msg);
	if (type==JK_AJP13_SEND_BODY_CHUNK) {
		unsigned short chunked_length ;
		if(!last_msg->getShort(&chunked_length)){
			return NULL;
		}
		if(chunked_length>last_msg->getLen()){
			return NULL;
		}
		len = chunked_length;
		if (len==0) {
			bodyEnd = true;		
			return NULL;
		}
		return last_msg->getBytes();
	}
	return NULL;
}
//解析body
Parse_Result KAjpFetchObject::parseBody(KHttpRequest *rq,char *data,int len)
{
	//printf("parse body len=%d\n",len);
	parsed_len = 0;
	hot = data + len;
	return Parse_Continue;
}
KAjpMessage * KAjpFetchObject::parse(char **str,int len)
{
	//printf("parse len = %d\n",len);
	if (body_len==-1) {
		//head
		if (len<=0) {
			return NULL;
		}
		assert(header_len<4);
		int left_read = 4 - header_len;
		left_read = MIN(left_read,len);
		memcpy(((char *)ajp_header)+header_len,*str,left_read);
		header_len += left_read;
		*str += left_read;
		len -= left_read;
		//add_buf(str,len);
		if (header_len<4) {
			//continue;
			return NULL;
		}
		if(!((ajp_header[0]==0x41 && ajp_header[1]==0x42) || (ajp_header[0]==0x12 && ajp_header[1]==0x34))){
			klog(KLOG_ERR,"recv wrong package.\n");
			return NULL;
			//goto done;
		}
		body_len = ajp_header[2] << 8 | ajp_header[3];
		if (body_len>AJP_PACKAGE) {
			klog(KLOG_ERR,"recv wrong package length %d.\n",body_len);
			return NULL;
		}
		assert(body == NULL);
		body = (char *)malloc(body_len);
		body_hot = body;
	}
	int this_body_len = MIN(len,body_len);
	memcpy(body_hot,*str,this_body_len);
	body_len -= this_body_len;
	*str += this_body_len;
	body_hot += this_body_len;
	if (body_len==0) {
		//switch to read head
		KAjpMessage *msg = new KAjpMessage(body,body_hot-body);
		body_len = -1;
		header_len = 0;
		body = NULL;
		body_hot = NULL;
		return msg;
	}
	//len = this_body_len;
	return NULL;
}
