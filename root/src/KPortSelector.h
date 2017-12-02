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
#ifndef KPortSELECTOR_H_
#define KPortSELECTOR_H_

#include "global.h"

#ifdef HAVE_PORT_H
#include <port.h>
#include "KSelector.h"
#include "malloc_debug.h"

class KPortSelector : public KSelector
{
public:
	const char *getName()
	{
		return "port";
	}
	KPortSelector();
	virtual ~KPortSelector();
	void select();
	bool listen(KServer *st,resultEvent result);
	bool read(KSelectable *st,resultEvent result,bufferEvent buffer,void *arg);
        bool write(KSelectable *st,resultEvent result,bufferEvent buffer,void *arg);
        bool next(KSelectable *st,resultEvent result,void *arg);
	bool connect(KSelectable *st,resultEvent result,void *arg);
protected:
	void removeSocket(KSelectable *rq);
private:
	int kdpfd;
};
#endif
#endif /*KEPOLLSELECTOR_H_*/
