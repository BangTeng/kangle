#ifndef KHttpManageInclude_randomsdflisf97sd9f09s8df
#define KHttpManageInclude_randomsdflisf97sd9f09s8df

#include <map>
#include <string>
#include "global.h"
#include "do_config.h"
#include "log.h"
#include "http.h"
#include "malloc_debug.h"
#include "KLastModify.h"
#include "KUrlValue.h"
#include "KSyncFetchObject.h"
enum {
	USER_TYPE_UNAUTH, USER_TYPE_ADMIN, USER_TYPE_VIRTUALHOST, USER_TYPE_NORMAL
};
class KHttpManage : public KSyncFetchObject {
public:
	bool sendHttpContent();
	bool sendHttpHeader();
	KHttpManage();
	~KHttpManage();
	void process(KHttpRequest *rq);
	bool start(bool &hit);
	bool start_vhs(bool &hit);
	bool start_listen(bool &hit);
	bool start_obj(bool &hit);
	bool start_access(bool &hit);

	std::string getUrlValue(std::string name);
	bool sendHttp(const char *msg, INT64 content_length,
			const char * content_type = NULL, const char *add_header = NULL,
			int max_age = 0);
private:
	bool save_access(KVirtualHost *vh,std::string redirect_url);
	KHttpRequest *rq;
	std::map<std::string, std::string> urlParam;
	KUrlValue urlValue;
	char *postData;
	int postLen;
	int userType;
	bool sendHttp(const std::string &msg);
	void sendTest();
	bool parseUrlParam(char *param);
	bool parseUrl(char *url);
	void parsePostData();
	char *parsePostFile(int &len, std::string &fileName);
	bool exportConfig();
	bool importConfig();
	bool importexport();
	bool runCommand();
	bool sendErrorSaveConfig(int file=0);
	bool sendErrPage(const char *err_msg, int closed_flag = 0);
	bool sendMainPage();
	bool sendLeftMenu();
	bool sendMainFrame();
	bool sendProcessInfo();
	bool send_css();
	bool config();
	bool configsubmit();
	bool extends(unsigned item=0);
	bool sendRedirect(const char *newUrl);
	bool reboot();
	bool sendXML(const char *buf,bool addXml=false);
	//bool start_fetchobj();
	bool xml;
};
bool changeAdminPassword(KUrlValue *url,std::string &errMsg);
bool checkManageLogin(KHttpRequest *rq) ;
#endif

