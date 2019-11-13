#ifndef KDSOEXTENDMANAGE_H_99
#define KDSOEXTENDMANAGE_H_99
#include <map>
#include <vector>
#include "KDsoExtend.h"
#include "utils.h"

class KDsoExtendManage
{
public:
	KDsoExtendManage()
	{

	}
	~KDsoExtendManage()
	{

	}
	void html(std::stringstream &s);
	bool add(KDsoExtend *dso);
	bool add(std::map<std::string, std::string> &attribute);
	void ListTarget(std::vector<std::string> &target);
	KRedirect *RefsRedirect(std::string &name);
private:
	std::map<const char *, KDsoExtend *, lessp> dsos;
	KMutex lock;
};
#endif
