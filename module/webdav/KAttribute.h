/*
 * KAttribute.h
 *
 *  Created on: 2010-8-8
 *      Author: keengo
 */

#ifndef KATTRIBUTE_H_
#define KATTRIBUTE_H_
#include <map>
#include "utils.h"
typedef lessp attrp;

class KAttribute {
public:
	KAttribute();
	virtual ~KAttribute();
	const char *getValue(const char *name);
	void add(const char *name, const char *value);
	void add(char *name, char *value);
	void del(const char *name);
	std::map<char *, char *, attrp> atts;
};

#endif /* KATTRIBUTE_H_ */
