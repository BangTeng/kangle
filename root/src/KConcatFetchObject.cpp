#include "KConcatFetchObject.h"
#include "KHttpObject.h"
#include "http.h"
#include "KUrlParser.h"
void concatCallBack(KHttpRequest *rq,void *data,sub_request_action action)
{
	if (action==sub_request_free) {
		return;
	}
	rq->fetchObj->readBody(rq);
}
void KConcatFetchObject::open(KHttpRequest *rq)
{
	KFetchObject::open(rq);
	KHttpObject *obj = rq->ctx->obj;
	SET(obj->index.flags,ANSW_LOCAL_SERVER);
	if (rq->ctx->lastModified > 0 && rq->ctx->lastModified == rq->file->getLastModified()) {
		//		headResult = HEAD_NOT_MODIFIED;
		obj->data->status_code = STATUS_NOT_MODIFIED;
		handleUpstreamRecvedHead(rq);
		return;
	}
	//´¦Àícontent-type
	if (!stageContentType(rq,obj)) {
		handleError(rq,STATUS_FORBIDEN,"cann't find such content-type");
		return;
	}
	//ÉèÖÃlast-modified
	SET(obj->index.flags,ANSW_LAST_MODIFIED);
	obj->index.content_length = rq->file->fileSize;
	obj->index.last_modified = rq->file->getLastModified();
	char tmp_buf[42];
	mk1123time(obj->index.last_modified, tmp_buf, 41);
	obj->insertHttpHeader(kgl_expand_string("Last-Modified"),(const char *)tmp_buf,29);
	if (obj->data->status_code==0) {
		obj->data->status_code = STATUS_OK;
	}
	init(rq);
	handleUpstreamRecvedHead(rq);
}
void KConcatFetchObject::readBody(KHttpRequest *rq)
{
	KConcatPath *cp = hot;
	if (cp==NULL) {
		stage_rdata_end(rq,STREAM_WRITE_SUCCESS);
		return;
	}
	hot = hot->next;
	startRequest(rq,cp);
}
void KConcatFetchObject::init(KHttpRequest *rq)
{
	assert(head==NULL);
	KConcatPath *cp = new KConcatPath;
	cp->path = strdup(rq->url->path);
	add(cp);
	assert(rq->url->param);
	char *buf = strdup(rq->url->param+1);
	url_decode(buf,0,NULL,false);
	char *param = buf;
	while (param) {
		char *p = strchr(param,',');
		if (p) {
			*p = '\0';
			p++;
		}
		if (*param) {
			cp = new KConcatPath;
			cp->path = strdup(param);
			add(cp);
		}
		param = p;
	}
	free(buf);
	hot = head;
}
void KConcatFetchObject::startRequest(KHttpRequest *rq,KConcatPath *cp)
{
	KUrl *url = new KUrl;
	url->host = strdup(rq->url->host);
	url->port = rq->url->port;
	char *path = cp->path;
	if (*path=='/') {
		url->path = strdup(path);
	} else {
		char *src_path = strdup(rq->url->path);
		char *p = strrchr(src_path,'/');
		int src_path_len = 0;
		if (p) {
			p++;
			*p = '\0';
			src_path_len = p - src_path;				
		}
		url->path = (char *)malloc(src_path_len + 2 + strlen(path));
		if (src_path_len>0) {
			memcpy(url->path,src_path,src_path_len);
		}
		memcpy(url->path + src_path_len,path,strlen(path)+1);
		free(src_path);
	}
	char *param = strchr(url->path,'?');
	if (param) {
		*param = '\0';
		url->param = strdup(param+1);
	}
	rq->beginSubRequest(url,concatCallBack,NULL);
}

