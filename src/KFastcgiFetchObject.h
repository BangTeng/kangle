#ifndef KFASTCGIFETCHOBJECT_H_
#define KFASTCGIFETCHOBJECT_H_

#include "KFetchObject.h"
#include "KFastcgiUtils.h"
#include "KFileName.h"
#include "KSocket.h"
#include "KAsyncFetchObject.h"
#include "KHttpHeadPull.h"
#include "KHttpObjectParserHook.h"

class KFastcgiFetchObject: public KAsyncFetchObject {
public:
	KFastcgiFetchObject();
	virtual ~KFastcgiFetchObject();
protected:
	void buildHead(KHttpRequest *rq);
	Parse_Result parseHead(KHttpRequest *rq,char *buf,int len) ;
	char *nextBody(KHttpRequest *rq,int &len);
	void buildPost(KHttpRequest *rq);
	Parse_Result parseBody(KHttpRequest *rq,char *data,int len);
	virtual bool isExtend()
	{
		return false;
	}
	void expectDone(KHttpRequest *rq)
	{
		lifeTime = 0;		
		KAsyncFetchObject::expectDone(rq);
	}
	bool needTempFile()
	{
		return true;
	}
	bool checkContinueReadBody(KHttpRequest *rq)
	{
		return !bodyEnd;
	}
	void readBodyEnd(KHttpRequest *rq)
	{
		//Ҫ���ֳ����ӣ�������������ȫ�����ݣ��ڶ���body���п�����������Ҫ������
		//һ���ǿյ����ݰ���һ����END_REQUEST��
		for(int i=0;i<2;i++){
			if (bodyEnd) {
				break;
			}
			int len = (int)(end - hot);
			if (len<=0) {
				break;
			}
			parse(rq,&hot,len);
		}
	}
	void adjustBuffer(INT64 offset)
	{
		if (end) {
			end += offset;
		}
	}
private:
	void appendPostEnd();
	char *parse(KHttpRequest *rq,char **str,int &len);
	char *end;
	int buf_len;
	FCGI_Header buf;	
	//body_len = -1ʱ��ʾ�ڶ�head
	int body_len;
	KHttpHeadPull parser;
	KHttpObjectParserHook hook;
	bool bodyEnd;
protected:
};
#endif /* KFASTCGIFETCHOBJECT_H_ */
