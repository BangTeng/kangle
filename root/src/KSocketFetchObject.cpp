/*
 * KSocketFetchObject.cpp
 *
 *  Created on: 2010-4-23
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


#include "do_config.h"
#include "lib.h"
#include "http.h"
#include "log.h"
#include "KHttpProtocolParser.h"
#include "KHttpObjectParserHook.h"
#include "KSocketFetchObject.h"
#include "malloc_debug.h"
