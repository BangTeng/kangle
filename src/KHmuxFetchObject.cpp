#include "KHmuxFetchObject.h"
#include "http.h"
#include "KRewriteMarkEx.h"
void KHmuxFetchObject::buildHead(KHttpRequest *rq)
{
	buffer = new KSocketBuffer(NBUFF_SIZE);
	char tmpbuff[50];
	addEnv(HMUX_URL,rq->url->path);
	addEnv(HMUX_METHOD,rq->getMethod());
	addEnv(CSE_PROTOCOL,"HTTP/1.1");
	if(rq->url->param){
		addEnv(CSE_QUERY_STRING,rq->url->param);
	}
	addEnv(HMUX_SERVER_NAME, rq->url->host);
	addEnv(CSE_SERVER_PORT, rq->c->socket->get_self_port());
	if (rq->client_ip) {
		addEnv(CSE_REMOTE_HOST,rq->client_ip);
		addEnv(CSE_REMOTE_ADDR,rq->client_ip);
	} else {
		char ips[MAXIPLEN];
		rq->c->socket->get_remote_ip(ips,sizeof(ips));
		addEnv(CSE_REMOTE_HOST,ips);
		addEnv(CSE_REMOTE_ADDR,ips);
	}
	addEnv(CSE_REMOTE_PORT,rq->c->socket->get_self_port());
	KHttpHeader *av = rq->parser.getHeaders();
	while (av) {
		if (strncasecmp(av->attr, "Proxy-", 6) == 0) {
			goto do_not_insert;
		}
		if (TEST(rq->flags,RQ_HAVE_EXPECT) && is_attr(av, "Expect")) {
			goto do_not_insert;
		}
		if (is_attr(av, "Accept-Encoding")) {
			goto do_not_insert;
		}
		if (is_internal_header(av)) {
			goto do_not_insert;
		}
		if (is_attr(av,"Content-Type")) {
			addEnv(CSE_CONTENT_TYPE,av->attr);
		} else {
			addEnv(HMUX_HEADER,av->attr);
		}
		addEnv(HMUX_STRING,av->val);
do_not_insert:
		av = av->next;
	}
	if (rq->content_length > 0) {
		addEnv(CSE_CONTENT_LENGTH, (char *)int2string(rq->content_length, tmpbuff));
	}
	if (rq->ctx->lastModified != 0) {
		char mk1123buff[50];
		mk1123time(rq->ctx->lastModified, mk1123buff, sizeof(mk1123buff));
		addEnv(HMUX_HEADER,"If-Modified-Since");
		addEnv(HMUX_STRING, mk1123buff);
	}
	
	if (rq->pre_post_length>0) {
		//处理pre loaded post数据
		addEnv(CSE_DATA,rq->parser.body,rq->pre_post_length);
		rq->parser.body += rq->pre_post_length;
		rq->parser.bodyLen -= rq->pre_post_length;
		rq->left_read -= rq->pre_post_length;
		rq->pre_post_length = 0;
	}
	if (rq->left_read==0) {
		buffer->write_byte(HMUX_QUIT);
	}
}
void KHmuxFetchObject::buildPost(KHttpRequest *rq)
{
	int length = (int)buffer->getLen();
	buff *buf = (buff *)malloc(sizeof(buff));
	buf->data = (char *)malloc(3);
	buf->used = 3;
	buf->data[0] = CSE_DATA;
	buf->data[1] = 	(length >> 8) & 0xff;
	buf->data[2] = (length) & 0xff;
	buffer->insertBuffer(buf);
	if(rq->left_read==0){
		buffer->write_byte(HMUX_QUIT);
	}
}
Parse_Result KHmuxFetchObject::parseHead(KHttpRequest *rq,char *data,int len)
{
	KHttpObject *obj = rq->ctx->obj;
	KHttpObjectParserHook hook(obj,rq);
	data = header + parsed_len;
	len = hot - header - parsed_len;
	hook.setProto(Proto_hmux);
	int last_http_header_len = 0;
	for (;;) {
		int get_len = 0;
		if (*data==CSE_DATA) {
			parsed_len = data - header;
			return Parse_Success;
		}
		char *msg = parse(&data,len,get_len);
		if (msg==NULL) {
			break;
		}
		//printf("msg=[%s]\n",msg);
		switch(code){
		case HMUX_STATUS:
			obj->data->status_code = atoi(msg);
			break;
		case HMUX_HEADER:
			//save the header wait the string
			if(last_http_header){
				break;
			}
			last_http_header_len = get_len;
			last_http_header = (char *)malloc(get_len+1);
			memcpy(last_http_header,msg,get_len);
			last_http_header[get_len] = '\0';
			break;
		case HMUX_STRING:
			if(last_http_header==NULL){
				break;
			}
			char *val = (char *)malloc(get_len+1);
			memcpy(val,msg,get_len);
			val[get_len] = '\0';
			if(hook.parseHeader(last_http_header,val,get_len,false)==HTTP_PARSE_SUCCESS){
				obj->insertHttpHeader2(last_http_header,last_http_header_len,val,get_len);
			} else {
				free(last_http_header);
				free(val);
			}
			last_http_header = NULL;
			break;
		}
	}
	//更新parsed_len
	parsed_len = data - header;

	return Parse_Continue;
}
bool KHmuxFetchObject::addEnv(const char code, const char *val,int length)
{
	char temp[4];
	temp[0] = code;
	temp[1] = (length >> 8) & 0xff;
	temp[2] = (length) & 0xff;

	buffer->write_all(temp,3);
	buffer->write_all(val,length);
	return true;
}
bool KHmuxFetchObject::addEnv(const char code, const char *val)
{
	return addEnv(code,val,strlen(val));
}
bool KHmuxFetchObject::addEnv(const char code, int i)
{
	char temp[8];

	temp[0] = code;
	temp[1] = 0;
	temp[2] = 4;
	temp[3] = (char) (i >> 24);
	temp[4] = (char) (i >> 16);
	temp[5] = (char) (i >> 8);
	temp[6] = (char) (i);
	buffer->write_all(temp,7);
	return true;
}
char *KHmuxFetchObject::parse(char **str,int &len,int &get_len)
{
	if (body_len==-1) {
		if(header_len==0 && *str[0] == HMUX_QUIT){
			code = *str[0];
			len--;
			(*str) ++ ;
			bodyend = true;
			return NULL;
		}
		if(len>=3 && header_len==0){
			//fast
			code = *str[0];
			int l1 = (*str)[1] & 0xff;
			int l2 = (*str)[2] & 0xff;	
			body_len = (l1 << 8) + l2;
			len -= 3;
			*str += 3;
		} else {
			if (len<=0) {
				return NULL;
			}
			int left_read = 3 - header_len;
			memcpy(hmux_header + header_len,*str,left_read);
			header_len += left_read;
			*str += left_read;
			len -= left_read;
			if (header_len<3) {
				return NULL;
			}
			code = hmux_header[0];
			int l1 = hmux_header[1] & 0xff;
			int l2 = hmux_header[2] & 0xff;
			body_len = (l1 << 8) + l2;
		}
	}
	if (code!=CSE_DATA && len<body_len) {
		//不完整
		return NULL;
	}
	char *body = *str;
	assert(body_len>=0);
	get_len = MIN(len,body_len);
	body_len -= get_len;
	*str += get_len;
	len -= get_len;
	if (body_len<=0) {
		assert(body_len==0);
		body_len = -1;
		header_len = 0;
	}
	return body;
}
