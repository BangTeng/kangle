#ifndef KASYNCSELECTABLE_H
#define KASYNCSELECTABLE_H
#include "KSelectable.h"
#include "KFile.h"
class KAsyncSelectable : public KSelectable
{
public:
	KAsyncSelectable(KFile *fp)
	{
		memset(static_cast<KSelectable *>(this), 0, sizeof(KSelectable));
		this->fp = fp;
	}
	KSocket *getSocket()
	{
		return NULL;
	}
	void read(void *arg,resultEvent result,bufferEvent buffer);
private:
	KFile *fp;
};
#endif
