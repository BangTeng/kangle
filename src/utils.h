/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 *
 * kangle web server              http://www.kangleweb.net/
 * ---------------------------------------------------------------------
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *  See COPYING file for detail.
 *
 *  Author: KangHongjiu <keengo99@gmail.com>  2011-07-18
 */
#ifndef UTILS_H_93427598324987234kjh234k
#define UTILS_H_93427598324987234kjh234k
#include <stdio.h>
#ifndef _WIN32
#include <pthread.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>
#include <stdlib.h>

#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#endif
#include "do_config.h"
#include "global.h"
#include "KHttpRequest.h"
#include "forwin32.h"
#include "malloc_debug.h"
#include "KSelectable.h"
#include "KServer.h"
#include "KPipeStream.h"
#include "KCgiEnv.h"
#include "KWinCgiEnv.h"
#define RDSTD_NAME_PIPE   0
#define RDSTD_ALL         1
#define RDSTD_NONE        2
#define RDSTD_INPUT       3
struct lessp {
	bool operator()(const char * __x, const char * __y) const {
		return strcmp(__x, __y) < 0;
	}
};
struct lessp_icase {
	bool operator()(const char * __x, const char * __y) const {
		return strcasecmp(__x, __y) < 0;
	}
};
struct lessf {
	bool operator()(const char * __x, const char * __y) const {
		return filecmp(__x, __y) < 0;
	}
};
#ifdef _WIN32
typedef KWinCgiEnv KCmdEnv;
#else
typedef KCgiEnv KCmdEnv;
#endif
//#ifdef _WIN32
#define USER_T	std::string
//#else
//#define USER_T	int
//#endif
int get_path(char *argv0, std::string &path);
int get_param(int argc, char **argv, int &i,const char *param, char *value);
FUNC_TYPE FUNC_CALL time_thread(void *arg);
void closeAllConnection();
std::string b64encode(const unsigned char *in, int len = 0);
char *b64decode(const unsigned char *in, int *l);
char *my_strtok(char *msg, char split, char **ptrptr);
void explode_cmd(char *str,std::vector<char *> &result);
void explode(const char *str, const char split,
		std::vector<std::string> &result, int limit = -1);
void explode(const char *str, const char split,
		std::map<char *, bool, lessp> *result, int limit = -1);
std::string string2lower(std::string str);
buff *inflate_buff(buff *in_buf, INT64 &len, bool fast);
buff *deflate_buff(buff *in_buf, int level, INT64 &len, bool fast);
char *utf82charset(const char *str, size_t len, const char *charset);

FILE *fopen_as(const char *file, const char *mode, int uid, int gid);
int create_select_pipe(KHttpRequest *rq, KClientSocket *client, int tmo,
		int max_server_len = -1, int max_client_len = -1);
bool name2uid(const char *name, int &uid, int &gid);
bool name2gid(const char *name, int &gid);
#ifdef _WIN32
bool setCloseOnExec(HANDLE fd, bool closeExec);
#endif
wchar_t *toUnicode(const char *str,int len=0,int cp_code=0);
bool waitForRW(SOCKET sockfd, bool isWrite, int timeo);
bool startService(KListenHost *service, bool start = false);
void change_admin_password_crypt_type();
void loadExtConfigFile();
void buildAttribute(char *buf, std::map<char *, char *, lessp_icase> &attibute);
void split(char *buf, std::vector<char *> &item);
std::string endTag();
void addCurrentEnv(KCmdEnv *env);
/*
����һ�������⹤�������ȴ����
*/
bool startProcessWork(Token_t token, char * args[], KCmdEnv *envs);
/*
����һ������.
rdstd = 0 ʹ��namedPipe
rdstd = 1 �ض���std
rdstd = 2 ������
*/
KPipeStream * createProcess(Token_t token,char * args[],KCmdEnv *envs, int rdstd);
/*
����һ������.
rdstd = 0 ʹ��namedPipe
rdstd = 1 �ض���std
rdstd = 2 ������
*/
bool createProcess(KPipeStream *st,Token_t token, char * args[], KCmdEnv *envs, int rdstd);
bool createProcess(Token_t token, char * args[],KCmdEnv *envs,char *cur_dir,PIPE_T in,PIPE_T out,PIPE_T err,pid_t &pid);
pid_t createProcess(Token_t token,const char *cmd,KCmdEnv *envs,const char *curdir,kgl_process_std *std);
bool killProcess(KVirtualHost *vh);
#ifdef _WIN32
extern KMutex closeExecLock;
BOOL StartInteractiveClientProcess (
		HANDLE hToken,
		LPCSTR lpApplication,
		LPTSTR lpCommandLine ,
		KPipeStream *st,int isCgi,LPVOID env
);
BOOL init_winuser(bool first_run);
#define PATH_SPLIT_CHAR		'\\'
#else
#define PATH_SPLIT_CHAR		'/'
#endif
#define CRYPT_TYPE_PLAIN	0
#define CRYPT_TYPE_KMD5		1
#define CRYPT_TYPE_SALT_MD5 2
#define CRYPT_TYPE_SIGN     3
#ifdef ENABLE_HTPASSWD_CRYPT
#define CRYPT_TYPE_HTPASSWD 4
#define TOTAL_CRYPT_TYPE	5
#else
#define TOTAL_CRYPT_TYPE    4
#endif
inline int parseCryptType(const char *type) {
	if (strcasecmp(type, "md5") == 0) {
		return CRYPT_TYPE_KMD5;
	}
#ifdef ENABLE_HTPASSWD_CRYPT
	if (strcasecmp(type,"htpasswd")==0) {
		return CRYPT_TYPE_HTPASSWD;
	}
#endif
	if (strcasecmp(type,"smd5")==0) {
		return CRYPT_TYPE_SALT_MD5;
	}
	if (strcasecmp(type,"sign")==0) {
		return CRYPT_TYPE_SIGN;
	}
	return CRYPT_TYPE_PLAIN;
}
inline const char *buildCryptType(int type) {
	switch (type) {
	case CRYPT_TYPE_PLAIN:
		return "plain";
	case CRYPT_TYPE_KMD5:
		return "md5";
#ifdef ENABLE_HTPASSWD_CRYPT
	case CRYPT_TYPE_HTPASSWD:
		return "htpasswd";
#endif
	case CRYPT_TYPE_SALT_MD5:
		return "smd5";
	case CRYPT_TYPE_SIGN:
		return "sign";
	}
	return "unknow";
}
bool checkPassword(const char *toCheck, const char *password, int cryptType);
inline void string2lower2(char *str) {
	while (*str) {
		*str = tolower(*str);
		str++;
	}
}
inline const char *getWorkModelName(int model) {
	if (model == 0) {
		return "http";
	}
#ifdef IP_TRANSPARENT
#ifdef ENABLE_TPROXY
	if (TEST(model,WORK_MODEL_TPROXY|WORK_MODEL_TCP)== (WORK_MODEL_TPROXY | WORK_MODEL_TCP)) {
		return "tcp-tproxy";
	}
	if (TEST(model,WORK_MODEL_TPROXY)) {
		return "http-tproxy";
	}
#endif
#endif
	if (model == (WORK_MODEL_SSL | WORK_MODEL_MANAGE)) {
		return "manages";
	}
	if (model == WORK_MODEL_SSL) {
		return "https";
	}
#ifdef WORK_MODEL_TCP
	if (model == WORK_MODEL_TCP) {
		return "tcp";
	}
#endif
	return "manage";
}
inline bool parseWorkModel(const char * type, int &model) {
	model = 0;
#ifdef IP_TRANSPARENT
#ifdef ENABLE_TPROXY
	if (strcasecmp(type,"http-tproxy") == 0) {
		SET(model,WORK_MODEL_TPROXY);
		return true;
	}
	if (strcasecmp(type,"tcp-tproxy") == 0) {
		SET(model,WORK_MODEL_TPROXY|WORK_MODEL_TCP);
		return true;
	}

#endif
#endif
	if (strcasecmp(type, "https") == 0) {
		SET(model,WORK_MODEL_SSL);
	} else if (strcasecmp(type, "manage") == 0) {
		SET(model,WORK_MODEL_MANAGE);
	} else if (strcasecmp(type, "manages") == 0) {
		SET(model,WORK_MODEL_SSL|WORK_MODEL_MANAGE);
#ifdef WORK_MODEL_TCP
	} else if (strcasecmp(type,"portmap")==0 || strcasecmp(type, "tcp") == 0) {
		SET(model,WORK_MODEL_TCP);
#endif
	} else if (strcasecmp(type, "http") != 0) {
		fprintf(stderr, "cann't recognize the listen type=%s\n", type);
		return false;
	}
	return true;
}
inline char *getPath(const char *file) {
	char *path = strdup(file);
	char *e = path + strlen(path);
	while(e>path){
		if(*e=='/'
#ifdef _WIN32
		|| *e=='\\'
#endif
			){
				*e = '\0';
				break;
		}
		e--;

	}
	return path;

}
inline bool isAbsolutePath(const char *str) {
	if (str[0] == '/') {
		return true;
	}
#ifdef _WIN32
	if(str[0]=='\\') {
		return true;
	}
	if(strlen(str)>1 && str[1]==':') {
		return true;
	}
#endif
	return false;
}
/*
 * be sure path is ended with '/'
 */
inline void pathEnd(std::string &path) {
	bool pathEnded = false;
	if (path.size() == 0) {
		path = "/";
		return;
	}
	const char c = path[path.size() - 1];
	if (c == '/') {
		pathEnded = true;
	}
#ifdef _WIN32
	if(c=='\\') {
		pathEnded = true;
	}
#endif
	if (!pathEnded) {
		path = path + PATH_SPLIT_CHAR;
	}
}
extern const char *serverType;
inline const char *getServerType() {
	return serverType;
}
inline const char *getOsType()
{
#if defined(_WIN32)
	return "windows";
#elif defined(LINUX)
	return "linux";
#endif
	return "unix";
}
inline char *rand_password(int len)
{
	std::stringstream s;
	const char *base_password = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
	int base_len=(int)strlen(base_password);
    if(len<8){
        len=8;
    }
    for(int i=0;i<len;i++){
        s << base_password[rand()%base_len];
    }
    return strdup(s.str().c_str());
}


bool open_process_std(kgl_process_std *std,KFile *file);
void closeAllFile(int start_fd);
#endif
