/*
 * KSSICommandInclude.cpp
 *
 *  Created on: 2010-8-4
 *      Author: keengo
 */

#include "KSSICommandInclude.h"
#include "KSSIProcess.h"
#include "KVirtualHost.h"
#include "KSelector.h"

void includeCallBack(KHttpRequest *rq,void *data,sub_request_action action)
{	
	if (action==sub_request_free) {
		return;
	}
	rq->fetchObj->readBody(rq);
}
KSSICommandInclude::KSSICommandInclude() {
}

KSSICommandInclude::~KSSICommandInclude() {
}
Process_status KSSICommandInclude::process(KSSIContext *context, char *cmd, std::map<
		char *, char *, lessp_icase> &attribute, KWStream *out) {
	bool isVirtual = false;
	std::map<char *, char *, lessp_icase>::iterator it;
	it = attribute.find((char *)"file");
	if (it == attribute.end()) {
		it = attribute.find((char *)"virtual");
		if(it==attribute.end()){
			*out << "<!-- " << cmd << " command must have virtual or file attribute -->";
			return Process_success;
		}
		isVirtual = true;
	}
	if (context->getFileCount() + context->getRequest()->stackSize > 128) {
		*out << "<!-- " << cmd << " level have reached max[128] -->";
		return Process_success;
	}
	char *path = (*it).second;
	KHttpRequest *rq = context->getRequest();
	if (isVirtual) {
		//virtual是由子请求处理。
		bool qsa = false;
		it = attribute.find((char *)"qsa");
		if (it!=attribute.end() && strcmp((*it).second,"1")==0) {
			qsa = true;
		}
		isVirtual = true;
		KUrl *url = new KUrl;
		url->host = strdup(rq->url->host);
		url->port = rq->url->port;
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
		KStringBuf pp;
		if (param) {
			*param = '\0';
			pp << (param+1);
			if (qsa && rq->url->param) {
				pp << "&" << rq->url->param;
			}
		} else if(qsa && rq->url->param) {
			pp << rq->url->param;
		}
		if (pp.getSize()>0) {
			url->param = pp.stealString();
		}
		rq->beginSubRequest(url,includeCallBack,NULL);
		return Process_sub_request;
	}
	const char *docRoot = rq->svh->doc_root;
	int path_len = strlen(path);
	char *cur_path;	
	if (*path=='/') {
		cur_path = strdup(path);
	} else {
		int cur_path_len = strlen(context->curFile->path);
		cur_path = (char *) xmalloc(cur_path_len+path_len+1);
		memcpy(cur_path, context->curFile->path, cur_path_len + 1);
		char *hot = cur_path + cur_path_len;
		while (hot >= cur_path) {
			if (*hot == '/' || *hot == '\\') {
				hot[1] = '\0';
				break;
			}
			hot--;
		}
		cur_path_len = strlen(cur_path);
		memcpy(cur_path + cur_path_len, path, path_len + 1);
	}	
	char *triped_path = KFileName::tripDir2(cur_path, PATH_SPLIT_CHAR);
	char *new_path = rq->svh->vh->alias(true,triped_path);
	KFileName *file = new KFileName();
	bool result;
	if (new_path) {
		result = file->giveName(new_path);
	} else {
		result = file->setName(docRoot, triped_path, rq->getFollowLink());
	}
	if (!result) {
		*out << "<!-- cann't open ssi file [" << cur_path << "] -->";
	}
	xfree(triped_path);
	if (result) {
		if(strcasecmp(cmd,"flastmod")==0){
			*out << context->getTime(file->getLastModified(),false);
		}else if(strcasecmp(cmd,"fsize")==0){
			*out << context->getSize(file->fileSize);
		}else{
			context->pushFileContext(file, cur_path);
		}
	}
	xfree(cur_path);
	delete file;
	return Process_success;
}
