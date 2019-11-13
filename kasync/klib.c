#include <stdlib.h>
#include "klib.h"
#include "kforwin32.h"
#include "kfile.h"
void kgl_msleep(int msec) {
#if	defined(OSF)
	/* DU don't want to sleep in poll when number of descriptors is 0 */
	usleep(msec * 1000);
#elif	defined(_WIN32)
	Sleep(msec);
#else
	struct timeval tv;
	tv.tv_sec = msec / 1000;
	tv.tv_usec = (msec % 1000) * 1000;
	select(1, NULL, NULL, NULL, &tv);
#endif
}
