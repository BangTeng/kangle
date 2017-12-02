/*
 * KPathRedirect.h
 *
 *  Created on: 2010-6-12
 *      Author: keengo
 */

#ifndef KPATHREDIRECT_H_
#define KPATHREDIRECT_H_
#include "global.h"
#include "KFileName.h"
#include "KHttpKeyValue.h"
#include "KCountable.h"
#include "KRedirect.h"
#include <string>
#include "KXml.h"
#include "KReg.h"

struct KParamItem
{
	std::string name;
	std::string value;
};
class KRedirectMethods
{
public:
	void setMethod(const char *methodstr);
	std::string getMethod()
	{
		std::stringstream s;
		if(all_method){
			s << "*";
		}else{
			for(int i=1;i<MAX_METHOD;i++){
				if(methods[i]){
					s << KHttpKeyValue::getMethod(i) << ",";
				}
			}
		}
		return s.str();
	}
	bool matchMethod(char method)
	{
		if(all_method){
			return true;
		}
		return methods[(int)method]==1;
	}
private:
	char methods[MAX_METHOD];
	bool all_method;
};
class KBaseRedirect : public KCountableEx {
public:
	KBaseRedirect() {
		rd = NULL;
		inherited = false;
		confirmFile = true;
	}
	/**
	* 调用此处rd必须先行addRef。
	*/
	KBaseRedirect(KRedirect *rd,int confirmFile) {
		inherited = false;
		this->rd = rd;
		this->confirmFile = confirmFile;
	}
	void buildXML(std::stringstream &s)
	{
		s << " extend='";
		if(rd){
			const char *rd_type = rd->getType();
			if (strcmp(rd_type, "mserver") == 0) {
				rd_type = "server";
			}
			s << rd_type << ":" << rd->name;
		}else{
			s << "default";
		}
		s << "'";
		if (!confirmFile) {
			s << " confirm_file='0'";
		}
		s << " allow_method='" << allowMethod.getMethod() << "'";
		
		s << "/>\n";
	}
	
	KRedirectMethods allowMethod;
	bool inherited;
	int confirmFile;
	KRedirect *rd;
protected:
	~KBaseRedirect() {
		if (rd) {
			rd->release();
		}
	}
};
class KPathRedirect : public KBaseRedirect {
public:
	KPathRedirect(const char *path,KRedirect *rd);
	bool match(const char *path,int len);
	char *path;
	int path_len;
protected:
	~KPathRedirect();
private:
	KReg *reg;
};

#endif /* KPATHREDIRECT_H_ */
