/*
 * KSubVirtualHost.cpp
 *
 *  Created on: 2010-9-2
 *      Author: keengo
 */
#include <vector>
#include <string.h>
#include <sstream>
#include "KVirtualHost.h"
#include "KSubVirtualHost.h"
#include "KHtAccess.h"
#include "http.h"
#include "KCdnContainer.h"
#include "KHttpProxyFetchObject.h"
#include "malloc_debug.h"
std::string htaccess_filename;
using namespace std;
KSubVirtualHost::KSubVirtualHost(KVirtualHost *vh) {
	this->vh = vh;
	host = NULL;
	dir = NULL;
	doc_root = NULL;
#ifdef ENABLE_SUBDIR_PROXY
	
	ip = NULL;
	dst = NULL;
	type = subdir_local;
	lifeTime = 0;
	http_proxy = NULL;
	https_proxy = NULL;
#endif	
	allSuccess = true;
	fromTemplete = false;
	bind_host = NULL;
	wide = false;
	
}
KSubVirtualHost::~KSubVirtualHost() {
	if (host) {
		xfree(host);
	}
	if (bind_host) {
		xfree(bind_host);
	}
	if (dir) {
		xfree(dir);
	}
	if (doc_root) {
		xfree(doc_root);
	}
#ifdef ENABLE_SUBDIR_PROXY
	if (dst) {
		dst->destroy();
		delete dst;
	}
	if (http_proxy) {
		free(http_proxy);
	}
	if (https_proxy) {
		free(https_proxy);
	}
#endif
	
}
void KSubVirtualHost::release()
{
#ifdef ENABLE_VH_RS_LIMIT
		vh->releaseConnection();
#endif
		vh->destroy();
}

bool KSubVirtualHost::setHost(const char *host)
{
	wide = false;
	if (bind_host) {
		xfree(bind_host);
		bind_host = NULL;
	}
	if (*host=='*') {
		host ++;
		wide = true;
	}
	if (*host=='.') {
		wide = true;
	}
	assert(this->host==NULL);
	this->host = xstrdup(host);
	char *p = strchr(this->host,'|');
	if (p) {
		*p = '\0';
		this->dir = strdup(p+1);
	}
	int bind_host_len = strlen(this->host);
	this->bind_host = (domain_t)malloc(bind_host_len+2);
	bool result = revert_hostname(this->host,bind_host_len,this->bind_host,bind_host_len+2);
	if (!result) {
		klog(KLOG_ERR,"bind hostname [%s] is error\n",this->host);
	}
	return result;
}
/* 如果setHost里面设置了dir信息(|分隔),以setHost的为准 */
void KSubVirtualHost::setDocRoot(const char *doc_root, const char *dir) {
#ifndef ENABLE_SUB_VIRTUALHOST
	dir = NULL;
#endif
	if (this->dir==NULL) {
		if (dir == NULL) {
			this->dir = xstrdup("/");
		} else {
			this->dir = xstrdup(dir);
		}
	}
	char *ssl_crt = strchr(this->dir, KGL_SSL_PARAM_SPLIT_CHAR);
	if (ssl_crt != NULL) {
		*ssl_crt = '\0';
		
	}
#ifdef ENABLE_SUBDIR_PROXY
	if (strncasecmp(this->dir,"http://",7)==0) {
		type = subdir_proxy;
		if (http_proxy) {
			free(http_proxy);
			http_proxy = NULL;
		}
		if (https_proxy) {
			free(https_proxy);
			https_proxy = NULL;
		}
		this->doc_root = strdup(doc_root);
		if (dst) {
			dst->destroy();
		} else {
			dst = new KUrl;
		}
		if (!parse_url(this->dir,dst)) {
			dst->destroy();
			delete dst;
			dst = NULL;
			klog(KLOG_ERR,"cann't parse url [%s]\n",this->dir);
		}
		if (dst && dst->param) {
			char *t = strstr(dst->param,"life_time=");
			if (t) {
				lifeTime = atoi(t+10);
			}
			ip = strstr(dst->param,"ip=");
		
			if (ip) {
				ip += 3;
				char *t = strchr(ip,'&');
				if (t) {
					*t = '\0';
				}
			}
		}
		return;
	} else if (strncasecmp(this->dir, "server://", 9) == 0) {
		type = subdir_proxy;
		if (http_proxy) {
			free(http_proxy);
			http_proxy = NULL;
		}
		if (https_proxy) {
			free(https_proxy);
			https_proxy = NULL;
		}
		http_proxy = strdup(this->dir + 9);
		char *p = strchr(http_proxy, '|');
		if (p) {
			*p = '\0';
			https_proxy = strdup(p + 1);
		} else {
			https_proxy = strdup(http_proxy);
		}
		return;
	}
	type = subdir_local;
#endif
	KFileName::tripDir3(this->dir,'/');
	char *sub_doc_root = KFileName::concatDir(doc_root, this->dir);
#ifdef ENABLE_VH_RUN_AS
	if(vh->add_dir.size()>0){
		this->doc_root = KFileName::concatDir(sub_doc_root,vh->add_dir.c_str());
		xfree(sub_doc_root);
	}else{
		this->doc_root = sub_doc_root;
	}
#else
	this->doc_root = sub_doc_root;
#endif

	size_t doc_len = strlen(this->doc_root);
	if(this->doc_root[doc_len-1]!='/'
#ifdef _WIN32
		&& this->doc_root[doc_len-1]!='\\'
#endif
		){
		sub_doc_root = (char *)xmalloc(doc_len+2);
		memcpy(sub_doc_root,this->doc_root,doc_len);
		sub_doc_root[doc_len] = PATH_SPLIT_CHAR;
		sub_doc_root[doc_len+1] = '\0';
		xfree(this->doc_root);
		this->doc_root = sub_doc_root;
	}
	KFileName::tripDir3(this->doc_root,PATH_SPLIT_CHAR);
}
bool KSubVirtualHost::bindFile(KHttpRequest *rq, KHttpObject *obj,bool &exsit,KAccess **htresponse,bool &handled) {
	//	char *tripedDir = KFileName::tripDir2(rq->url->path, '/');
#ifdef _WIN32
	char *c = rq->url->path + strlen(rq->url->path) - 1;
	if(*c=='.' || *c==' '){
		return false;
	}
#endif
	if (!TEST(rq->workModel,WORK_MODEL_INTERNAL) && vh->htaccess.size()>0 && doc_root) {
		char *path = xstrdup(rq->url->path);
		int prefix_len = 0;
		for (;;) {
			char *hot = strrchr(path, '/');
			if (hot == NULL) {
				break;
			}
			if (prefix_len == 0) {
				prefix_len = hot - path;
			}
			*hot = '\0';
			char *apath = vh->alias(TEST(rq->workModel,WORK_MODEL_INTERNAL)>0,path);
			KFileName htfile;
			bool htfile_exsit;
			if (apath) {
				htfile_exsit = htfile.setName(apath, vh->htaccess.c_str(), 0);
				xfree(apath);
			} else {
				stringstream s;
				s << doc_root << path;
				htfile_exsit
					= htfile.setName(s.str().c_str(), vh->htaccess.c_str(), 0);
			}
			if (htfile_exsit) {
				if ((*htresponse)==NULL) {
					(*htresponse) = new KAccess;
					(*htresponse)->setType(RESPONSE);
				}
				KAccess *htrequest = new KAccess;
				htrequest->setType(REQUEST);
				if (makeHtaccess(path,&htfile,htrequest,*htresponse)) {
					if (htrequest->check(rq,obj)==JUMP_DENY) {
						handled = true;
						xfree(path);
						delete htrequest;
						delete (*htresponse);
						*htresponse = NULL;
						stageDeniedRequest(rq);
						return true;
					}
				}
				delete htrequest;
			}
			//todo:check rebind file
			//	if(filencmp(,rq->url->path)
		}
		xfree(path);
	}
	if (rq->file) {
		//重新绑定过,因为有可能重写了
		delete rq->file;
		rq->file = NULL;
	}
#ifdef ENABLE_SUBDIR_PROXY
	if (type == subdir_proxy) {
		if (rq->fetchObj != NULL) {
			return true;
		}
		if (http_proxy) {
			KRedirect *rd = NULL;
			if (TEST(rq->raw_url.flags, KGL_URL_SSL)) {
				 rd = cdnContainer.refsRedirect(https_proxy);
			} else {
				rd = cdnContainer.refsRedirect(http_proxy);
			}
			if (rd) {
				KFetchObject *fo = rd->makeFetchObject(rq,rq->file);
				KBaseRedirect *brd = new KBaseRedirect(rd, false);
				fo->bindBaseRedirect(brd);
				brd->release();
				rq->fetchObj = fo;
			}
			return true;
		}
		if (dst && dst->host) {
			if (*(dst->host)=='-') {
				rq->fetchObj = new KHttpProxyFetchObject();
				return true;
			}
			const char *tssl = NULL;
			
			int tport = dst->port;
			if (dst->port == 0) {
				tport = rq->url->port;
				
			}
			rq->fetchObj = cdnContainer.get(ip,dst->host,tport,tssl,lifeTime);
			return true;
		}
	}
#endif
	return bindFile(rq,exsit,false,true);
}
bool KSubVirtualHost::bindFile(KHttpRequest *rq,bool &exsit,bool searchDefaultFile,bool searchAlias)
{
	KFileName *file = new KFileName;
	if (!searchAlias || !vh->alias(TEST(rq->workModel,WORK_MODEL_INTERNAL)>0,rq->url->path,file,exsit,rq->getFollowLink())) {
		exsit = file->setName(doc_root, rq->url->path, rq->getFollowLink());
	}
	kassert(rq->file == NULL);
	rq->file = file;
	if (searchDefaultFile && file->isDirectory()) {
		KFileName *defaultFile = NULL;
		if (vh->getIndexFile(rq,file,&defaultFile,NULL)) {
			delete rq->file;
			rq->file = defaultFile;
		}
	}
	return true;
}
bool KSubVirtualHost::makeHtaccess(const char *prefix,KFileName *file,KAccess *request,KAccess *response)
{
	KApacheConfig htaccess(true);
	htaccess.setPrefix(prefix);
	std::stringstream s;
	if (htaccess.load(file,s)) {
		KXml xmlParser;
		xmlParser.addEvent(request);
		xmlParser.addEvent(response);
		bool result=false;
		try {
			result = xmlParser.parseString(s.str().c_str());
		} catch(KXmlException &e) {
			fprintf(stderr,"%s",e.what());
			return false;
		}
		return result;
	}
	return false;
}
char *KSubVirtualHost::mapFile(const char *path) {
	char *new_path = vh->alias(true,path);
	if (new_path) {
		return new_path;
	}
	return KFileName::concatDir(doc_root, path);
}

