/*
 * KPathRedirect.cpp
 *
 *  Created on: 2010-6-12
 *      Author: keengo
 */
#include "utils.h"
#include "KPathRedirect.h"
#include "malloc_debug.h"
void KRedirectMethods::setMethod(const char *methodstr)
{
	memset(methods,0,sizeof(methods));
	if(methodstr==NULL || *methodstr=='\0' || strcmp(methodstr,"*")==0){
		all_method = true;
	}else{
		all_method = false;
		char *buf = strdup(methodstr);
		char *hot = buf;
		for(;;){
			char *p = strchr(hot,',');
			if(p){
				*p = '\0';
			}
			methods[KHttpKeyValue::getMethod(hot)] = 1;
			if(p==NULL){
				break;
			}else{
				hot = p+1;
			}
		}
		xfree(buf);
	}
}
KPathRedirect::KPathRedirect(const char *path, KRedirect *rd) : KBaseRedirect(rd,0){
	int path_len = strlen(path);
	char *hot;
	this->path = (char *)xmalloc(path_len+1);
	hot = this->path;
	this->path_len = path_len;
	memcpy(hot,path,path_len);
	hot[path_len] = '\0';
	if (*path=='~') {
		reg = new KReg;
#ifdef _WIN32
		int flag = PCRE_CASELESS;
#else
		int flag = 0;
#endif
		reg->setModel(path+1,flag);
	} else {
		reg = NULL;
	}
}

KPathRedirect::~KPathRedirect() {
	if (path) {
		xfree(path);
	}
	if (reg) {
		delete reg;
	}
}

bool KPathRedirect::match(const char *path,int len) {
	if (reg) {
		return reg->match(path,len,0)>0;
	}
	return filencmp(path,this->path,path_len)==0;
}
