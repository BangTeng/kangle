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
 *  Author: KangHongjiu <keengo99@gmail.com>
 */
#ifndef KSOCKETBUFFER_H
#define KSOCKETBUFFER_H
#include "KStream.h"
#include "forwin32.h"
#include "KBuffer.h"
#ifdef _WIN32
#pragma warning(disable:4200)
#endif
class KSocketBuffer : public KStream
{
public:
	KSocketBuffer(int chunk_size)
	{
		hot_buf = NULL;
		head = NULL;
		hot = NULL;
		totalLen = 0;
		this->chunk_size = chunk_size;
	}
	KSocketBuffer()
	{
		hot_buf = NULL;
		head = NULL;
		hot = NULL;
		totalLen = 0;
		this->chunk_size = NBUFF_SIZE;
	}
	~KSocketBuffer()
	{
		while(head){
			buff *next = head->next;
			free(head->data);
			free(head);
			head = next;
		}
	}
	//调用getRBuffer之前要调用startRead切换到读模式
	void getRBuffer(LPWSABUF buffer,int &bufferCount);
	char *getRBuffer(int &len);
	bool readSuccess(int got);
	void writeSuccess(int got);
	char *getWBuffer(int &len);
	void nextStep(int got)
	{
		assert(hot_buf && hot);
		hot += got;
	}
	inline void appendBuffer(buff *buf)
	{
		if (hot_buf==NULL) {
			assert(head == NULL);
			head = buf;
		} else {
			assert(head);
			assert(hot_buf->next == NULL);
			hot_buf->next = buf;
		}
		buf->next = NULL;
		hot_buf = buf;
		totalLen += buf->used;
	}
	template<typename T>
	inline T *append()
	{
		buff *buf = (buff *)malloc(sizeof(buff));
		buf->data = (char *)malloc(sizeof(T));
		buf->used = sizeof(T);
		appendBuffer(buf);
		return (T *)buf->data;		
	}
	template<typename T>
	inline T *insert()
	{
		buff *buf = (buff *)malloc(sizeof(buff));
		buf->data = (char *)malloc(sizeof(T));
		buf->used = sizeof(T);
		totalLen += buf->used;
		buf->next = head;
		head = buf;
		return (T *)buf->data;		
	}
	inline void insertBuffer(buff *buf)
	{
		buf->next = head;
		head = buf;
		totalLen += buf->used;
	}
	//暂时未实现read
	int read(char *buf,int len);
	void write_byte(int ch)
	{
		char temp[2];
		temp[0] = ch;
		write_all(temp,1);
	}
	StreamState write_all(const char *buf, int len);
	//切换到读模式,返回总大小
	inline unsigned startRead()
	{
		hot_buf = head;
		if (hot_buf) {
			hot = hot_buf->data;
		} else {
			hot = NULL;
		}
		return totalLen;
	}
	inline void print()
	{
		buff *tmp = head;
		while(tmp){
			if(tmp->used>0){
				fwrite(tmp->data,1,tmp->used,stdout);
			}
			tmp = tmp->next;
		}
	}
	inline void destroy()
	{
		while(head){
			buff *next = head->next;
			free(head->data);
			free(head);
			head = next;
		}
		hot_buf = NULL;
		hot = NULL;
		totalLen = 0;
	}
	unsigned getLen()
	{
		return totalLen;
	}
	buff *getHead()
	{
		return head;
	}
	buff *getHot()
	{
		return hot_buf;
	}
	//调试要用，要判断private值
	friend class KFastcgiFetchObject;
private:
	inline buff *newbuff()
	{
		buff *buf = (buff *)malloc(sizeof(buff));
		buf->data = (char *)malloc(chunk_size);
		buf->used = 0;
		buf->next = NULL;
		return buf;
	}
	buff *hot_buf;
	buff *head;
	char *hot;
	unsigned totalLen;
	int chunk_size;
};
#endif
