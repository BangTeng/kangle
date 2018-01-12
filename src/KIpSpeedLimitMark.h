#ifndef KIPSPEEDLIMITMARK_H
#define KIPSPEEDLIMITMARK_H
#include <map>
#include "KMark.h"
#include "KSpeedLimit.h"
#include "utils.h"
class KIpSpeedLimitMark;
struct KIpSpeedLimitContext
{
	char *ip;
	KIpSpeedLimitMark *mark;
};
void WINAPI ip_speed_limit_clean(void *data);
class KIpSpeedLimitMark : public KMark
{
public:
	KIpSpeedLimitMark()
	{
		speed_limit = 0;
	}
	virtual ~KIpSpeedLimitMark()
	{
		std::map<char *,KSpeedLimit *,lessp>::iterator it;
		for (it=ips.begin();it!=ips.end();it++) {
			(*it).second->release();
			free((*it).first);
		}
	}
	bool supportRuntime()
	{
		return true;
	}
	void requestClean(char *ip)
	{
		lock.Lock();
		std::map<char *,KSpeedLimit *,lessp>::iterator it;
		it = ips.find(ip);
		if (it!=ips.end() && (*it).second->getRef()<=1) {
			free((*it).first);
			(*it).second->release();
			ips.erase(it);
		}
		lock.Unlock();	
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj, const int chainJumpType,
			int &jumpType)
	{	
		if (rq->client_ip==NULL) {
			rq->client_ip = (char *)malloc(MAXIPLEN);
			rq->c->socket->get_remote_ip(rq->client_ip,MAXIPLEN);
		}
		lock.Lock();
		std::map<char *,KSpeedLimit *,lessp>::iterator it;
		it = ips.find(rq->client_ip);
		KSpeedLimit *sl = NULL;
		if (it==ips.end()) {
			sl = new KSpeedLimit;
			sl->setSpeedLimit(speed_limit);
			ips.insert(std::pair<char *,KSpeedLimit *>(strdup(rq->client_ip),sl));
		} else {
			sl = (*it).second;
		}
		sl->addRef();
		lock.Unlock();
		rq->pushSpeedLimit(sl);
		KIpSpeedLimitContext *speed_limit_context = new KIpSpeedLimitContext();
		speed_limit_context->ip = strdup(rq->client_ip);
		speed_limit_context->mark = this;
		addRef();
		rq->registerRequestCleanHook(ip_speed_limit_clean,speed_limit_context);
		return true;
	}
	KMark *newInstance()
	{
		return new KIpSpeedLimitMark;
	}
	const char *getName()
	{
		return "ip_speed_limit";
	}
	std::string getHtml(KModel *model)
	{
		KIpSpeedLimitMark *m = (KIpSpeedLimitMark *)model;
		std::stringstream s;
		s << "speed_limit:<input name='speed_limit' value='";
		if (m) {
			s << get_size(m->speed_limit);
		}
		s << "'> / second";
		return s.str();
	}
	std::string getDisplay()
	{
		std::stringstream s;
		s << get_size(speed_limit) << "/second ,record:";
		lock.Lock();
		s << ips.size();
		lock.Unlock();
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute)
			throw (KHtmlSupportException)
	{
		speed_limit = (int)get_size(attribute["speed_limit"].c_str());
	}
	void buildXML(std::stringstream &s)
	{
		s << "speed_limit='" << get_size(speed_limit) << "'>";
	}
private:
	int speed_limit;
	std::map<char *,KSpeedLimit *,lessp> ips;
	KMutex lock;
};
#endif
