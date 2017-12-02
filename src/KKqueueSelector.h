#ifndef KKQUEUESELECTOR_H_
#define KKQUEUESELECTOR_H_

#include "global.h"

#ifdef BSD_OS
#include <sys/event.h>
#include "KSelector.h"
#include "malloc_debug.h"

class KKqueueSelector : public KSelector
{
public:
	const char *getName()
	{
		return "kqueue";
	}
	KKqueueSelector();
	virtual ~KKqueueSelector();
	void select();
protected:
	bool next(KSelectable *st,resultEvent result,void *arg);
	bool listen(KServer *st,resultEvent result);
	void removeSocket(KSelectable *st);
	bool read(KSelectable *st,resultEvent result,bufferEvent buffer,void *arg) ;
	bool write(KSelectable *st,resultEvent result,bufferEvent buffer,void *arg);
	bool connect(KSelectable *st,resultEvent result,void *arg);

private:
	int kdpfd;

};
#endif
#endif /*KEPOLLSELECTOR_H_*/
