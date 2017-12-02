/*
 * KChildListen.h
 *
 *  Created on: 2010-7-10
 *      Author: keengo
 * kangle�ӽ��̼��������࣬kangle�ӽ���ͨ�������˿ڹ����������ӡ�
 * һ�����ӽ����ͻ�Ѹ����Ӽ��뵽ѡ�����С�
 */

#ifndef KCHILDLISTEN_H_
#define KCHILDLISTEN_H_
#include <string>
#include "KSelectable.h"
#include "KSocket.h"
#include "KApiRedirect.h"
class KChildListen {
public:
	KChildListen();
	virtual ~KChildListen();
	void unlink_unix()
	{
		if(unix_path.size()>0){
			unlink(unix_path.c_str());			
		}
	}
	ReadState canRead();
	KPipeStream *st;
	KServerSocket *server;
	std::string unix_path;
};
extern KChildListen *cl;
#endif /* KCHILDLISTEN_H_ */
