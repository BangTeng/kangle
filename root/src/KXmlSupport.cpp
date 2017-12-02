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
#include <stdlib.h>
#include <string.h>
#include <vector>
#include "KXmlSupport.h"
#include "malloc_debug.h"
#include "KXml.h"

using namespace std;

KXmlSupport::KXmlSupport()
{
}

KXmlSupport::~KXmlSupport()
{
}

bool KXmlParser::parse(string file,KStream *err)
{
	KXml xml;
	xml.setEvent(this);
	try{
		xml.parseFile(file);
	}catch(KXmlException e){
		err->operator <<(e.what());
		return false;
	}
	return true;
}
