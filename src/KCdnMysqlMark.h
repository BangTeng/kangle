/*
 * KCdnMysqlMark.h
 *
 *  Created on: 2010-11-9
 *      Author: keengo
 */

#ifndef KCDNMYSQLMARK_H_
#define KCDNMYSQLMARK_H_
#include "utils.h"
#include "KMark.h"
#include "KSingleAcserver.h"
#include "KMutex.h"
#include "kmysql.h"
#include "KCdnContainer.h"
#ifdef ENABLE_CDN_MYSQL
class KCdnMysqlMark: public KMark {
public:
	KCdnMysqlMark();
	virtual ~KCdnMysqlMark();
	bool mark(KHttpRequest *rq, KHttpObject *obj, const int chainJumpType,
			int &jumpType);
	KMark *newInstance();
	const char *getName();
	std::string getHtml(KModel *model);
	std::string getDisplay();
	void editHtml(std::map<std::string, std::string> &attribute)
			throw (KHtmlSupportException);
	void buildXML(std::stringstream &s);
	bool syncData();
	bool syncAllData(KMysql *mysql);
	void cleanUnrefsTarget();
//	KSingleAcserver *getSingleServer(const char *host,int port);
	void freeServer(std::map<char *, CdnTarget *, lessp_icase> *servers);
	int flush_time;
private:
	std::string host;
	int port;
	std::string user;
	std::string password;
	std::string db;
	int life_time;
	bool threadStarted;
	INT64 lastUpdate;
	std::map<char *, CdnTarget *, lessp_icase> *servers;
	KMutex lock;
	std::string lastMsg;
};
#endif
#endif /* KCDNMYSQLMARK_H_ */

