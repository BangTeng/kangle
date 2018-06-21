/*
 * KEpollNoticeSelectable.h
 *
 *      Author: keengo
 */

#ifndef SRC_KEPOLLNOTICESELECTABLE_H_
#define SRC_KEPOLLNOTICESELECTABLE_H_
#include "KSelectable.h"
#ifdef BSD_OS
#include <sys/types.h>
#include <sys/event.h>
#endif
class KEpollSelector;
class KKqueueSelector;
class KEpollNoticeSelectable : public KSelectable
{
public:
#ifdef LINUX
	KEpollNoticeSelectable(KEpollSelector *selector);
#elif BSD_OS
	KEpollNoticeSelectable(KKqueueSelector *selector);
	friend class KKqueueSelector;	
#endif
	~KEpollNoticeSelectable();
	bool notice(KSelectable *st,int got);
	void event();
private:
	KMutex lock;
	kgl_list *notice_list;
#ifdef BSD_OS
	struct kevent notify;
#endif
};
#endif /* SRC_KEPOLLNOTICESELECTABLE_H_ */
