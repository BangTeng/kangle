#ifndef KSERVERLISTEN_H_
#define KSERVERLISTEN_H_
#include <list>
#include "utils.h"

class KServerListen
{
public:
	KServerListen();
	static void start(std::vector<KServer *> &serverList);
	static bool start(KServer *server);
	virtual ~KServerListen();
	static FUNC_TYPE FUNC_CALL serverThread(void *param);

};

#endif /*KSERVERLISTEN_H_*/
