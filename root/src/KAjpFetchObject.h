/*
 * KAjpFetchObject.h
 * implement ajp/1.3 protocol
 *  Created on: 2010-7-31
 *      Author: keengo
 */

#ifndef KAJPFETCHOBJECT_H_
#define KAJPFETCHOBJECT_H_
#include "global.h"
#include "KAsyncFetchObject.h"
#include "KAcserver.h"
#include "KFileName.h"
#include "KAjpMessage.h"

class KAjpFetchObject: public KAsyncFetchObject {
public:
	KAjpFetchObject();
	virtual ~KAjpFetchObject();
	//int sendHead(KHttpRequest *rq, KHttpObject *obj, time_t lastModified);
	//void close(KHttpRequest *rq, bool realClose);
	//bool write(const char *buf, int len);
protected:
	//��������ͷ��buffer�С�
	void buildHead(KHttpRequest *rq);
	//����head
	Parse_Result parseHead(KHttpRequest *rq,char *data,int len);
	//����post���ݵ�buffer�С�
	void buildPost(KHttpRequest *rq);
	//��ȡbody����
	char *nextBody(KHttpRequest *rq,int &len);
	//����body
	Parse_Result parseBody(KHttpRequest *rq,char *data,int len);
	void appendPostEnd();
	bool checkContinueReadBody(KHttpRequest *rq)
	{
		return !bodyEnd;
	}
private:
	KAjpMessage *parse(char **str,int len);
	unsigned char parseMessage(KHttpRequest *rq,KAjpMessage *msg);
	unsigned char ajp_header[4];	
	int header_len;
	char *body;
	char *body_hot;
	int body_len;
	int parsed_len;
	unsigned char reuse;
	KAjpMessage *last_msg;
	char *end;
	bool bodyEnd;
};

#endif /* KAJPFETCHOBJECT_H_ */
