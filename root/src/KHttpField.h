/*
 * KHttpField.h
 *
 *  Created on: 2010-7-15
 *      Author: keengo
 */

#ifndef KHTTPFIELD_H_
#define KHTTPFIELD_H_
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "global.h"
#include "KHttpHeader.h"
class http_field_t
{
public:
	http_field_t()
	{
		attr = NULL;
		val = NULL;
	}
	char *parse(char *attr)
	{
		while(*attr && isspace((unsigned char)*attr))
			attr++;
		this->attr = attr;
		char *hot = attr;
		bool haveval = false;
		while (*hot) {
			if(*hot=='='){
                                *hot = '\0';
                                hot++;
                                haveval = true;
                                break;
                        }
			if(*hot==','){
				*hot = '\0';
				hot++;
				break;
			}
			if(isspace((unsigned char)*hot)){
				*hot = '\0';
				hot++;
				char *p = strchr(hot,'=');
				if (p) {
					hot = (p+1);
					haveval = true;
					break;
				}
			}
			hot++;
		}
		if(haveval){
			while(*hot && isspace((unsigned char)*hot))
				hot++;
			if(*hot=='"'){
				//���ַ���ֵ
				hot++;
				val = hot;
				char *p = strchr(hot,'"');				
				if(p){
					*p = '\0';
				}
				hot = p+1;
			}else{
				//����
				val = hot;
			}
			while(*hot){
				if(*hot == ','){
					*hot = '\0';
					hot++;
					break;
				}
				if(isspace((unsigned char)*hot)){
					*hot = '\0';
				}
				hot++;
			}
		}else{
			val = NULL;
		}
		return hot;
	}
	http_field_t(char *attr,char *val)
	{
		this->attr = attr;
		this->val = val;
	}
	~http_field_t()
	{

	}
	char *attr;
	char *val;
	http_field_t *next;
};
class KHttpField {
public:
	KHttpField();
	virtual ~KHttpField();
	void parse(const char *str);
	http_field_t *getHeader()
	{
		return header;
	}
private:
	http_field_t *header;
	char *buf;
};

#endif /* KHTTPFIELD_H_ */
