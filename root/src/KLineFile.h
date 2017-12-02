/*
 * KLineFile.h
 *
 *  Created on: 2010-5-30
 *      Author: keengo
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */

#ifndef KLINEFILE_H_
#define KLINEFILE_H_
#include <string.h>
#include <time.h>

#include "global.h"
#include "KFile.h"
#define KGL_STREAM_FILE_SIZE 4096
enum OpenState
{
	OPEN_NOT_MODIFIED,
	OPEN_FAILED,
	OPEN_SUCCESS,
	OPEN_UNKNOW
};
/*
 * ���ж�ȡ�ļ�
 */
class KLineFile {
public:
	KLineFile();
	virtual ~KLineFile();
	OpenState open(const char *file,time_t &lastModified);
	void init(const char *str);
	void give(char *str);

	char *readLine();
private:
	char *buf;
	char *hot;

};
class KStreamFile {
public:
	bool open(const char *file,const char split_char='\n');
	char *read();
private:
	char *internelRead();
	char buf[KGL_STREAM_FILE_SIZE];
	char *hot;
	char split_char;
	KFile fp;
};
#endif /* KLINEFILE_H_ */
