/*
 * KCgiEnv.cpp
 *
 *  Created on: 2010-6-11
 *      Author: keengo
 */
#include <string.h>
#include <stdio.h>
#include "forwin32.h"
#include "KCgiEnv.h"
#include "malloc_debug.h"
using namespace std;
KCgiEnv::KCgiEnv() {
	env = NULL;
}

KCgiEnv::~KCgiEnv() {
	list<char *>::iterator it;
	for (it = m_env.begin(); it != m_env.end(); it++) {
		xfree(*it);
	}
	if(env){
		xfree(env);
	}
}
bool KCgiEnv::addEnv(const char *attr, const char *val) {
	int len = strlen(attr) + strlen(val) + 1;
	char *str = (char *) xmalloc(len + 1);
	if (str == NULL) {
		return false;
	}
	str[len] = '\0';
	snprintf(str, len + 1, "%s=%s", attr, val);
	m_env.push_back(str);
	return true;
}
bool KCgiEnv::addEnv(const char *env) {
	m_env.push_back(xstrdup(env));
	return true;
}
char **KCgiEnv::getEnv() {
	return env;
}
bool KCgiEnv::addEnvEnd() 
{
	assert(env==NULL);
	if(env){
		xfree(env);
	}
	env = (char **) xmalloc(m_env.size() * (sizeof(char *) + 1));
	list<char *>::iterator it;
	int i;
	for (i = 0, it = m_env.begin(); it != m_env.end(); it++, i++) {
		env[i] = (*it);
	}
	env[i] = NULL;
	return true;
}

