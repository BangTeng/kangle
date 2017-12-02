/*
 * KSSIContext.h
 *
 *  Created on: 2010-8-2
 *      Author: keengo
 */

#ifndef KSSICONTEXT_H_
#define KSSICONTEXT_H_
#include <list>
#include <assert.h>
#include "utils.h"
#include "KHttpRequest.h"
#include "KFileName.h"
#include "KEnvInterface.h"
#include "KFile.h"
#define SSI_START_STRING "<!--#"
#define SSI_END_STRING   "-->"

enum sizefmt_t
{
	sizefmt_abbrev,
	sizefmt_bytes
};
class KSSIProcess;
struct KSSICondition {
	bool parentCondition;
	bool condition;
	bool haveCondition;
};
/*
 * 当前ssi文件的环境上下文
 */
class KSSIFileContext {
public:
	KSSIFileContext(char *path) {
		this->path = strdup(path);
		buffer = NULL;
		hot = NULL;
		haveCondition = false;
		condition = true;
		parentCondition = true;
		left_size = 0;
		file_left_size = 0;
	}
	~KSSIFileContext();
	bool init(KFileName *file);
	bool readData();
	bool condition;
	bool haveCondition;
	bool parentCondition;
	void pushCondition();
	bool popCondition();
	int getConditionSize() {
		return conditionStack.size();
	}
	char *getBlockBuffer(int &len,bool &cmd);
	char *buffer;
	char *hot;
	int left_size;
	int file_left_size;
	KFileName *file;
	char *path;
private:
	KFile fp;
	std::list<KSSICondition *> conditionStack;
};
/*
 * 整个ssi请求的环境上下文
 */
class KSSIContext : public KEnvInterface{
public:
	KSSIContext();
	virtual ~KSSIContext();
	friend class KSSIProcess;
	KHttpRequest *getRequest() {
		return rq;
	}
	void setRequest(KHttpRequest *rq) {
		this->rq = rq;
	}
	bool isCondition() {
		assert(curFile);
		return curFile->condition;
	}
	KSSIFileContext *curFile;
	int getFileCount() {
		return fileStack.size();
	}
	bool pushFileContext(KFileName *file, char *path);
	bool popFileContext();
	void setProcessor(KSSIProcess *processor) {
		this->processor = processor;
	}
	KSSIProcess *getProcessor() {
		return processor;
	}
	KFileName *getFirstFile();
	char *parseString(const char *str);
	bool printEnv(KWStream *s);
	bool addEnv(const char *attr,const char *val);
	void setValue(const char *val, const char *value);
	const char *getValue(const char *val);
	const char *getSystemValue(const char *val);
	const char *getSystemValue2(const char *val);
	const char *getSize(INT64 size);
	const char *getSize(INT64 size,int base);
	const char *getSizeBytes(INT64 size);
	bool setSizeFmt(const char *sizefmt);
	bool setTimeFmt(const char *timefmt);
	bool setErrMsg(const char *errmsg);
	const char *getTime(time_t time,bool gmt);
private:
	KHttpRequest *rq;
	KSSIProcess *processor;
	std::list<KSSIFileContext *> fileStack;
	std::map<char *,char *,lessp_icase> vars;
	std::map<char *,char *,lessp_icase> envs;
	sizefmt_t sizefmt;
	std::string timefmt;
	std::string errmsg;
	std::string sizestr;
};
#endif /* KSSICONTEXT_H_ */
