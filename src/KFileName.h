/*
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
#ifndef KFileName_h_slkdjf981223
#define KFileName_h_slkdjf981223
#include <string>
#ifndef _WIN32
#include <sys/stat.h> 
#endif
#include "global.h"
#include "forwin32.h"

enum CheckLinkState 
{
	CheckLinkFailed,
	CheckLinkContinue,
	CheckLinkIsFile
};
class KFileName {
public:
	KFileName();
	~KFileName();
	/*
	 调用这个函数之前请对于path,请先调用 tripDir2函数
	 */
	bool setName(const char *docRoot, const char *triped_path, int follow_link);
	bool setName(const char *path);
	bool giveName(char *path);
	const char *getName();
#ifdef _WIN32
	const wchar_t *getNameW();
#endif
	char *saveName();
	void restoreName(char *n);
	size_t getNameLen();
	CheckLinkState checkLink(const char *path, int follow_link);
	bool operator ==(KFileName &a);
	static bool tripDir(std::string &dir);
	static char *tripDir2(const char *dir, const char split_char);
	static void tripDir3(char *path,const char split_char);
	static char *concatDir(const char *doc_root,const char *file);
	bool getFileInfo();
	bool isDirectory();
	bool isPrevDirectory()
	{
		return prev_dir;
	}
	bool canExecute();
	time_t getLastModified() const;
	INT64 fileSize;
	static char *makeExt(const char *file);
	const char *getExt();
	const char *getIndex(){
		return index;
	}
	void setIndex(const char *index);
	bool isLinkChecked()
	{
		return linkChecked;
	}
	unsigned getPathInfoLength()
	{
		return pathInfoLength;
	}
private:
#ifdef _WIN32
	wchar_t *wname;
#endif
	char *name;
	char *ext;
	char *index;
	size_t name_len;
	struct _stat64 buf;
	bool prev_dir;
	bool linkChecked;
	//path_info时，url的长度
	unsigned pathInfoLength;
};
#endif
