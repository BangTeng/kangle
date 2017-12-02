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
/*
 * KCgiEnv.h
 *
 *  Created on: 2010-6-11
 *      Author: keengo
 * Ϊunix��cgi�ṩenv������࣬�سɻ�������
 */

#ifndef KCGIENV_H_
#define KCGIENV_H_
#include <list>
#include "KEnvInterface.h"

class KCgiEnv: public KEnvInterface {
public:
	KCgiEnv();
	virtual ~KCgiEnv();
	bool addEnv(const char *attr, const char *val);
	bool addEnv(const char *env);
	bool addEnvEnd();
	char **getEnv();
private:
	std::list<char *> m_env;
	char **env;
};

#endif /* KCGIENV_H_ */
