/*
 * KRedirect.h
 *
 *  Created on: 2010-6-12
 *      Author: keengo
 */

#ifndef KREDIRECT_H_
#define KREDIRECT_H_
#include "KFileName.h"
#include "KJump.h"
#include "KSocket.h"

class KHttpRequest;
class KFetchObject;
class KRedirect: public KJump {
public:
	KRedirect();
	virtual ~KRedirect();
	virtual KFetchObject *makeFetchObject(KHttpRequest *rq, KFileName *file) = 0;
	virtual void connect(KHttpRequest *rq) 
	{
		//����ִ�е�����
		assert(false);
	}
	void setEnable() {
		enable = true;
	}
	void setDisable() {
		enable = false;
	}
	virtual const char *getType() = 0;
	/*
	   get the redirect infomation.
	*/
	virtual const char *getInfo()
	{
		return "";
	}
	bool enable;
	/*
	 * �Ƿ�������չ������ǾͲ��ñ�����config.xml
	 */
	bool ext;
};

#endif /* KREDIRECT_H_ */
