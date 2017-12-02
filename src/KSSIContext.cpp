/*
 * KSSIContext.cpp
 *
 *  Created on: 2010-8-2
 *      Author: keengo
 */
#include "KSSIContext.h"
#include "malloc_debug.h"
#include "http.h"
using namespace std;
#define DEFAULT_TIME_FMT "%A, %d-%b-%Y %H:%M:%S %Z"
static const char *extra_vars[]={
	"DATE_LOCAL",
	"DATE_GMT",
	"LAST_MODIFIED",
	NULL
};
KSSIContext::KSSIContext() {
	curFile = NULL;
	//timestr = NULL;
	setTimeFmt(DEFAULT_TIME_FMT);
	sizefmt = sizefmt_bytes;
	//timefmt = DEFAULT_TIME_FMT;
}
KSSIContext::~KSSIContext() {
	list<KSSIFileContext *>::iterator it;
	for (it = fileStack.begin(); it != fileStack.end(); it++) {
		delete (*it);
	}
	if (curFile) {
		delete curFile;
	}
	std::map<char *, char *, lessp_icase>::iterator it2;
	for (it2 = vars.begin(); it2 != vars.end(); it2++) {
		xfree((*it2).first);
		xfree((*it2).second);
	}
	for (it2 = envs.begin(); it2 != envs.end(); it2++) {
		xfree((*it2).first);
		xfree((*it2).second);
	}
}
const char *KSSIContext::getSizeBytes(INT64 size)
{
		char buf[64];
		sizestr = int2string(size,buf);
		return sizestr.c_str();
	
}
const char *KSSIContext::getSize(INT64 size,int base)
{
	 stringstream s;
	 INT64 abbrevSize = (size >> base);
	 INT64 leftSize = size - (abbrevSize<<base);
	 if(base>10){
		 leftSize = (leftSize >> (base-10));
	 }
	 leftSize/=100;
	 s << abbrevSize ;
	 if(leftSize>0){
		 s << "." << leftSize;
	 }
	 switch(base){
		 case 40:
			 s << "T";
			 break;
		 case 30:
			 s << "G";
			 break;
		 case 20:
			 s << "M";
			 break;
		 case 10:
			 s << "K";
			 break;
	 }
	 s.str().swap(sizestr);
	 return sizestr.c_str();
}
const char *KSSIContext::getSize(INT64 size)
{
	if(sizefmt == sizefmt_bytes){
		return getSizeBytes(size);
	}/*
	if(size > (1<<40) ){
		return getSize(size,40);
	}
	*/
	if(size > (1<<30) ){
		return getSize(size,30);
	}
	if(size > (1<<20) ){
		return getSize(size,20);
	}
	if(size > (1<<10)){
		return getSize(size,10);
	}
	return getSizeBytes(size);
}
const char *KSSIContext::getTime(time_t time,bool gmt)
{
	char ts[8192];
	char tf[8192];
	char *strp;
	const char *fmt = timefmt.c_str();
	struct tm t;
	if(gmt){
		gmtime_r(&time, &t);
		 const char *f;
		 for(strp = tf, f = fmt; strp < tf + sizeof(tf) - 6 && (*strp = *f)
				; f++, strp++) {
				if (*f != '%') continue;
				switch (f[1]) {
				case '%':
					*++strp = *++f;
					break;
				case 'Z':
					*strp++ = 'G';
					*strp++ = 'M';
					*strp = 'T';
					f++;
					break;
				case 'z': /* common extension */
					*strp++ = '+';
					*strp++ = '0';
					*strp++ = '0';
					*strp++ = '0';
					*strp = '0';
					f++;
					break;
				}
			}
			*strp = '\0';
			fmt = tf;
	}else{
		localtime_r(&time,&t);
	}
	if(strftime(ts,8191,fmt,&t)==0){
		return NULL;
	}
	sizestr = ts;
	return sizestr.c_str();
}
bool KSSIContext::pushFileContext(KFileName *file, char *path) {
	if (file->fileSize>1048576*2) {
		//这个文件太大了2M.
		return false;
	}
	KSSIFileContext *fc = new KSSIFileContext(path);
	fc->file_left_size = (int)file->fileSize;
	if (!fc->init(file)) {
		delete fc;
		return false;
	}
	if (curFile != NULL) {
		fileStack.push_front(curFile);
	}
	curFile = fc;
	return true;
}
void KSSIContext::setValue(const char *val, const char *value) {
	std::map<char *, char *, lessp_icase>::iterator it;
	it = vars.find((char *) val);
	if (it == vars.end()) {
		if (value == NULL) {
			return;
		}
		vars.insert(pair<char *, char *> (xstrdup(val), xstrdup(value)));
	} else {
		xfree((*it).second);
		if (value == NULL) {
			xfree((*it).first);
			vars.erase(it);
			return;
		}
		//TODO:这里可能有bug.
		(*it).second = xstrdup(value);
	}
}
bool KSSIContext::printEnv(KWStream *s) {
	std::map<char *, char *, lessp_icase>::iterator it;
	for (it = vars.begin(); it != vars.end(); it++) {
		*s << (*it).first << "=" << (*it).second << "\n";
	}
	if (envs.size() == 0) {
		make_http_env(rq, NULL,0,getFirstFile(), this, false);
	}
	for (it = envs.begin(); it != envs.end(); it++) {
		*s << (*it).first << "=" << (*it).second << "\n";
	}
	for(int i=0;;i++){
		if(extra_vars[i]==NULL){
			break;
		}
		*s << extra_vars[i] << "=" << getSystemValue2(extra_vars[i]) << "\n";
	}
	return true;
}
const char *KSSIContext::getValue(const char *val) {
	std::map<char *, char *, lessp_icase>::iterator it;
	it = vars.find((char *) val);
	if (it == vars.end()) {
		return getSystemValue(val);
	}
	return (*it).second;
}
bool KSSIContext::popFileContext() {
	if (curFile == NULL) {
		return false;
	}
	delete curFile;
	list<KSSIFileContext *>::iterator it = fileStack.begin();
	if (it == fileStack.end()) {
		curFile = NULL;
		return false;
	}
	curFile = (*it);
	fileStack.pop_front();
	return true;
}
bool KSSIFileContext::readData()
{
	//assert(fp);
	int current_size;
	if (buffer==NULL) {
		assert(left_size==0);
		current_size = MIN(CHUNK,file_left_size);
		if (current_size<=0) {
			return false;
		}
		buffer = (char *) xmalloc(current_size+1);
		hot = buffer;
		left_size = fp.read(hot,current_size);
		if (left_size<=0) {
			return false;
		}
		file_left_size -= left_size;
		buffer[left_size] = '\0';
		return true;
	} else {
		assert(left_size>=0);
		assert(hot);
		int new_size = MIN(CHUNK,file_left_size);
		if (new_size<=0) {
			return false;
		}
		current_size = left_size + new_size;
		char *nb = (char *)malloc(current_size +1);
		if (left_size>0) {
			assert(hot);
			memcpy(nb,hot,left_size);
		}
		free(buffer);
		buffer = nb;
		hot = buffer;
		int read_size = fp.read(buffer + left_size,new_size);
		if (read_size<=0) {
			return false;
		}
		file_left_size -= read_size;
		left_size += read_size;
		buffer[left_size] = '\0';
		return true;
	}
}
bool KSSIFileContext::init(KFileName *file)
{
#ifdef _WIN32
	const wchar_t *wfilename = file->getNameW();
	if (wfilename) {
		fp.openW(wfilename,fileRead);
	}
#else
	const char *filename = file->getName();
	if (filename) {
		fp.open(filename,fileRead);
	}
#endif
	return fp.opened();
}
char *KSSIFileContext::getBlockBuffer(int &len,bool &cmd)
{
	if (left_size<=0 && !readData()) {
		if(left_size<=0){
			return NULL;
		}
		//last string
		len = left_size;
		cmd = false;
		left_size = 0;
		return hot;
	}
	if (strncmp(hot,SSI_START_STRING,sizeof(SSI_START_STRING)-1)==0) {
		for (;;) {
			char *end = strstr(hot,SSI_END_STRING);
			if (end) {
				*end = '\0';
				end += (sizeof(SSI_END_STRING) - 1) ;
				char *buf = hot;
				cmd = true;
				len = end - hot;
				left_size -= len;
				hot += len;
				return buf;
			}
			if ( !readData()) {
				len = left_size;
				cmd = false;
				left_size = 0;
				return hot;
			}
		}
	}
	char *ssi_start = strstr(hot,SSI_START_STRING);
	cmd = false;
	if (ssi_start==NULL) {
		len = left_size;
		left_size = 0;
		return hot;
	}
	len = ssi_start - hot;
	left_size -= len;
	char *buf = hot;
	hot += len;
	return buf;
}
KSSIFileContext::~KSSIFileContext() {
	list<KSSICondition *>::iterator it;
	for (it = conditionStack.begin(); it != conditionStack.end(); it++) {
		delete (*it);
	}
	if (buffer) {
		free(buffer);
	}	
	if(path){
		free(path);
	}
}
void KSSIFileContext::pushCondition() {
	KSSICondition *ssiCondition = new KSSICondition;
	ssiCondition->haveCondition = haveCondition;
	ssiCondition->condition = condition;
	ssiCondition->parentCondition = parentCondition;
	conditionStack.push_front(ssiCondition);
	haveCondition = false;
	parentCondition = condition;
}
bool KSSIFileContext::popCondition() {
	list<KSSICondition *>::iterator it = conditionStack.begin();
	if (it == conditionStack.end()) {
		return false;
	}
	KSSICondition *ssiCondition = (*it);
	haveCondition = ssiCondition->haveCondition;
	condition = ssiCondition->condition;
	parentCondition = ssiCondition->parentCondition;
	conditionStack.pop_front();
	delete ssiCondition;
	return true;
}
bool KSSIContext::addEnv(const char *attr, const char *val) {
	map<char *, char *, lessp_icase>::iterator it = envs.find((char *) attr);
	if (it != envs.end()) {
		xfree((*it).first);
		xfree((*it).second);
		envs.erase(it);
	}
	envs.insert(pair<char *, char *> (xstrdup(attr), xstrdup(val)));
	return true;
}
KFileName *KSSIContext::getFirstFile() {
	if (fileStack.size() == 0) {
		return curFile->file;
	}
	return (*fileStack.rend())->file;
}
const char * KSSIContext::getSystemValue(const char *val) {
	if (envs.size() == 0) {
		//first call;
		//printf("first call getSystemValue\n");
		make_http_env(rq,NULL, 0, getFirstFile(), this, false);
	}
	std::map<char *, char *, lessp_icase>::iterator it;
	it = envs.find((char *) val);
	if (it == envs.end()) {
		return getSystemValue2(val);
	}
	return (*it).second;
}
const char *KSSIContext::getSystemValue2(const char *val)
{
		if(strcasecmp(val,"DATE_LOCAL")==0){
			return getTime(time(NULL),false);
		}
		if(strcasecmp(val,"DATE_GMT")==0){
			return getTime(time(NULL),true);
		}
		if(strcasecmp(val,"LAST_MODIFIED")==0){
			return getTime(curFile->file->getLastModified(),false);
		}
		return NULL;
}
char *KSSIContext::parseString(const char *str) {
	KStringBuf s;
	if (str == NULL) {
		return NULL;
	}
	bool slash = false;
	char *buf = strdup(str);
	char *hot = buf;
	while (*hot) {
		if (slash) {
			s << *hot;
			slash = false;
			hot++;
		} else {
			if (*hot == '\\') {
				slash = true;
				hot++;
				continue;
			}
			if (*hot == '$') {
				char *env = hot + 1;
				if (hot[1] == '{') {
					env++;
					char *end = strchr(env, '}');
					if (end == NULL) {
						printf("cann't find end char `}`");
						return s.stealString();
					}
					*end = '\0';
					hot = end + 1;
				} else {
					char *end = env;
					while (*end) {
						if ((*env >= 'a' && *env <= 'z') || (*env >= 'A'
								&& *env <= 'Z') || (*env >= '0' && *env <= '9')
								|| *env == '_') {
							continue;
							env++;
						}
						*end = '\0';
						end++;
						break;
					}
					hot = end;
				}
				const char *val = getValue(env);
				if (val) {
					s << val;
				}
			} else {
				s << *hot;
				hot++;
			}
		}
	}
	xfree(buf);
	return s.stealString();
}
bool KSSIContext::setSizeFmt(const char *sizefmt) {
	if (strcasecmp(sizefmt, "bytes") == 0) {
		this->sizefmt = sizefmt_bytes;
		return true;
	} else if (strcasecmp(sizefmt, "abbrev") == 0) {
		this->sizefmt = sizefmt_abbrev;
		return true;
	}
	return false;
}
bool KSSIContext::setTimeFmt(const char *tf) {
	this->timefmt = tf;
	return true;
}
bool KSSIContext::setErrMsg(const char *errmsg) {
	this->errmsg = errmsg;
	return true;
}

