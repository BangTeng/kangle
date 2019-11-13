/*
 * test.cpp
 *
 *  Created on: 2010-5-31
 *      Author: keengo
 *
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
#include <vector>
#include <assert.h>
#include <string.h>
#include "global.h"
#include "lib.h"
#include "KGzip.h"
#include "do_config.h"
#include "kmalloc.h"
#include "KLineFile.h"
#include "KHttpFieldValue.h"
#include "KHttpField.h"
#include "KPipeStream.h"
#include "KFileName.h"
#include "KVirtualHost.h"
#include "KChunked.h"
#include "KHtAccess.h"
#include "KTimeMatch.h"
#include "KReg.h"
#include "kfile.h"
#include "KXml.h"
#include "KUrlParser.h"
#include "cache.h"
#include "kconnection.h"
#include "KHttpSink.h"
#include "kselector_manager.h"
#include "KHttpParser.h"

#ifdef ENABLE_INPUT_FILTER
#include "KMultiPartInputFilter.h"
#endif
using namespace std;
char *getString(char *str, char **substr);

void test_file()
{
	const char *test_file = "c:\\windows\\temp\\test.txt";
	KFile file;
	assert(file.open(test_file,fileWrite));
	assert(file.write("test",4));
	file.close();
	assert(file.open(test_file,fileRead));
	char buf[8];
	int len = file.read(buf,8);
	assert(len==4);
	assert(strncmp(buf,"test",4)==0);
	file.seek(1,seekBegin);
	len = file.read(buf,8);
	assert(len==3);
	file.close();
	file.open(test_file,fileAppend);
	file.write("t2",2);
	file.close();
	file.open(test_file,fileRead);
	assert(file.read(buf,8)==6);
	file.close();
}
bool test_pipe() {
	return true;
}
void test_regex() {
	KReg reg;
	reg.setModel("s",0);
	int ovector[6];
	int ret = reg.match("sjj",-1,PCRE_PARTIAL,ovector,6);
	//printf("ret=%d\n",ret);
	//KRegSubString *ss = reg.matchSubString("t", 1, 0);
	//assert(ss);
}
/*
void test_cache()
{
	const char *host = "abcdef";
	for (int i=0;i<100;i++) {
		std::stringstream s;
		int h = rand()%6;
		s << "http://" << host[h] << "/" << rand();
		KHttpObject *obj = new KHttpObject;
		create_http_object(obj,s.str().c_str(),false);
		obj->release();
	}
}
*/
void test_container() {
	/*
	unsigned char buf[18];
	const char *hostname = ".test.com";
	assert(revert_hostname(hostname,strlen(hostname),buf,sizeof(buf)));
	printf("ret=[%s]\n",buf);
	*/
}
void test_htaccess() {
	//static const char *file = "/home/keengo/httpd.conf";
	//KHtAccess htaccess;
	//KFileName file;
	//file.setName("/","/home/keengo/");
	//printf("result=%d\n",htaccess.load("/","/home/keengo/"));
}
void test_file(const char *path)
{
#if 0
	KFileName file;
	std::string doc_root = "d:\\project\\kangle\\www\\";
	bool result = file.setName(doc_root.c_str(), path, FOLLOW_LINK_NO|FOLLOW_PATH_INFO);
	printf("triped_path=[%s],result=%d\n",path,result);
	if(result){
		printf("pre_dir=%d,is_dir=%d,path_info=[%d],filename=[%s]\n",file.isPrevDirectory(),file.isDirectory(),file.getPathInfoLength(),file.getName());
	}
#endif
}
void test_files()
{
	test_file("/test.php");
	test_file("/test.php/a/b");
	test_file("/");
	test_file("/a/");
	test_file("/a");
	test_file("/b");

}
void test_timematch()
{
	KTimeMatch *t = new KTimeMatch;
	t->set("* * * * *");
	t->Show();
	delete t;
	t = new KTimeMatch;
	t->set("*/5  */5 * * *");
	t->Show();
	delete t;
	t = new KTimeMatch;
	t->set("2-10/3,50  0-6 * * *");
	t->Show();
	delete t;
}

void test_url_decode() {
	char buf2[5];
    //strcpy(buf2,"test");
    memcpy(buf2,"test",4);
    buf2[4] = '*';
    url_decode(buf2, 4,NULL,true);
   // printf("buf=[%s]\n",buf2);
    assert(buf2[4]=='*');
    //strcpy(buf2,"test");
    memcpy(buf2,"%20t",4);
    url_decode(buf2, 4,NULL,true);
    //printf("buf=[%s]\n",buf2);
    assert(buf2[2]=='\0');
}
#ifdef ENABLE_INPUT_FILTER
void test_dechunk() {
	KDechunkEngine *engine = new KDechunkEngine;
	int buf_len;
	const char *piece;
	int piece_length;
	const char *buf = "1\r\na\r\n";
	buf_len = strlen(buf);
	assert(engine->dechunk(&buf, buf_len, &piece, piece_length) == dechunk_success);
	assert(strncmp(piece, "a",piece_length) == 0);
	assert(engine->dechunk(&buf, buf_len, &piece, piece_length) == dechunk_continue);
	buf = "1";
	buf_len = 1;
	
	assert(engine->dechunk(&buf, buf_len, &piece, piece_length) == dechunk_continue);
	buf = "0\r\n012345";
	buf_len = strlen(buf);
	assert(engine->dechunk(&buf, buf_len, &piece, piece_length) == dechunk_continue);
	assert(piece_length==6 && strncmp(piece, "012345", piece_length) == 0);
	buf = "6789abcdef0\r\n";
	buf_len = strlen(buf);
	assert(engine->dechunk(&buf, buf_len, &piece, piece_length) == dechunk_success);
	assert(piece_length==10 && strncmp(piece, "6789abcdef", piece_length) == 0);
	delete engine;
}
#endif
void test_white_list() {
#ifdef ENABLE_BLACK_LIST
	kgl_current_sec = time(NULL);
	wlm.add(".", NULL, "1");
	wlm.add(".", NULL, "2");
	wlm.add(".", NULL, "3");
	wlm.flush(time(NULL)+100, 10);
#endif
}
void test_line_file()
{
	KStreamFile lf;
	lf.open("d:\\line.txt");
	for (;;) {
		char *line = lf.read();
		if (line == NULL) {
			break;
		}
		printf("line=[%s]\n", line);
	}
}
void test_suffix_corrupt() {
        char *str = new char[4];
        memcpy(str,"test1",5);
	delete[] str;
}
void test_prefix_corrupt() {
        char *str = (char *)malloc(4);
	void *pp = str - 1;
	memcpy(pp,"test",4);
        free(str);
}
void test_freed_memory() {
        char *str = (char *)malloc(4);
        free(str);
        memcpy(str,"test",4);
}
void test_http_parser()
{
	char *buf = strdup("HTTP/1.1 200 OK\r\na: bsssssssssssssddddd\r\n ttt");
	char *hot = buf;
	int len = strlen("HTTP/1.1 200 OK\r\na: bsssssssssssssddddd\r\n ttt");
	khttp_parser parser;
	khttp_parse_result rs;
	memset(&parser, 0, sizeof(parser));	
	kassert(kgl_parse_continue == khttp_parse(&parser, &hot, &len, &rs));
	kassert(kgl_parse_continue == khttp_parse(&parser, &hot, &len, &rs));	
}
bool test() {
	//printf("size=[%d]\n", kgl_align(1, 1024));
	//test_freed_memory();
	//test_suffix_corrupt();
	//test_prefix_corrupt();
	//printf("sizeof(kconnection) = %d\n",sizeof(kconnection));
	//printf("sizeof(KHttpSink)=%d\n",sizeof(KHttpSink));
	//printf("sizeof(KHttpRequest) = %d\n",sizeof(KHttpRequest));
	//printf("sizeof(pthread_mutex_t)=%d\n",sizeof(pthread_mutex_t));
	//printf("sizeof(lock)=%d\n",sizeof(KMutex));
	//test_cache();
	//test_file();
	//test_timematch();
	//test_xml();
	//printf("sizeof(kgl_str_t)=%d\n",sizeof(kgl_str_t));
	//test_ip_map();
	//test_line_file();
#ifdef ENABLE_HTTP2
	test_http2();
#endif
	//selectorManager.onReady(when_selector_manager_ready, NULL);
	kbuf b;
	b.flags = 0;
	test_url_decode();
	test_regex();
	test_htaccess();
	test_container();
#ifdef ENABLE_INPUT_FILTER
	test_dechunk();
#endif
	test_white_list();
	//test_http_parser();
	//printf("sizeof(KHttpRequest)=%d\n",sizeof(KHttpRequest));
	//	test_pipe();
	//printf("sizeof(obj)=%d,%d\n",sizeof(KHttpObject),sizeof(HttpObjectIndex));
	time_t nowTime = time(NULL);
	char timeStr[41];
	mk1123time(nowTime, timeStr, sizeof(timeStr) - 1);
	//printf("parse1123=%d\n",parse1123time(timeStr));
	assert(parse1123time(timeStr)==nowTime);
	INT64 t = 123;
	char buf[INT2STRING_LEN];
	int2string(t, buf);
	//printf("sizeof(sockaddr_i)=%d\n",sizeof(sockaddr_i));
	if (strcmp(buf, "123") != 0) {
		fprintf(stderr, "Warning int2string function is not correct\n");
		assert(false);
	} else if (string2int(buf) != 123) {
		fprintf(stderr, "Warning string2int function is not correct\n");
		assert(false);
	}
	KHttpField field;
//	test_files();
	
	return true;
}
