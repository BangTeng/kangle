#include "KQueueMark.h"
#ifdef ENABLE_REQUEST_QUEUE
#include "KRequestQueue.h"
#include "KAccess.h"
#include "http.h"
#define PER_MATCHER_SPLIT_CHAR ','
KQueueMark::~KQueueMark()
{
	if (queue) {
		queue->release();
		queue = NULL;
	}
}
bool KQueueMark::mark(KHttpRequest *rq, KHttpObject *obj,const int chainJumpType, int &jumpType)
{
	if (queue && rq->queue == NULL) {
		rq->ctx->queue_handled = false;
		rq->queue = queue;
		queue->addRef();
	}
	return true;
}
void KQueueMark::editHtml(std::map<std::string, std::string> &attribute) 
{
	int max_worker = atoi(attribute["max_worker"].c_str());
	int max_queue = atoi(attribute["max_queue"].c_str());
	if (queue == NULL) {
		queue = new KRequestQueue;
	}
	queue->set(max_worker, max_queue);
}
std::string KQueueMark::getDisplay()
{
	std::stringstream s;
	if (queue) {
		s << queue->getWorkerCount() << "/" << queue->getMaxWorker() << " " << queue->getQueueSize() << "/" << queue->getMaxQueue();
		s << " " << queue->getRef();
	}
	return s.str();
}
void KQueueMark::buildXML(std::stringstream &s)
{
	if (queue) {
		s << " max_worker='" << queue->getMaxWorker() << "' max_queue='" << queue->getMaxQueue();
	}
	s << "'>";
}
std::string KQueueMark::getHtml(KModel *model)
{
	KQueueMark *mark = (KQueueMark *)(model);
	std::stringstream s;
	s << "max_worker:<input name='max_worker' value='";
	if (mark && mark->queue) {
		s << mark->queue->getMaxWorker();
	}
	s << "'>";
	s << "max_queue:<input name='max_queue' value='";
	if (mark && mark->queue) {
		s << mark->queue->getMaxQueue();
	}
	s << "'>\n";

	return s.str();
}
KPerQueueMark::~KPerQueueMark()
{
	assert(queues.empty());
	while (matcher) {
		per_queue_matcher *next = matcher->next;
		delete matcher;
		matcher = next;
	}
}
bool KPerQueueMark::mark(KHttpRequest *rq, KHttpObject *obj, const int chainJumpType, int &jumpType)
{
	if (rq->queue) {
		return false;
	}
	KStringBuf *url = NULL;
	KRegSubString *ss = NULL;
	per_queue_matcher *matcher = this->matcher;
	while (matcher) {
		if (matcher->header == NULL) {
			if (url == NULL) {
				url = new KStringBuf;
				rq->url->getUrl(*url);
			}
			ss = matcher->reg.matchSubString(url->getString(), url->getSize(), 0);
		} else {
			KHttpHeader *av = rq->GetHeader();
			while (av) {
				if (attr_casecmp(av->attr, matcher->header) == 0 && av->val) {
					ss = matcher->reg.matchSubString(av->val, av->val_len, 0);
					break;
				}
				av = av->next;
			}
		}
		if (ss != NULL) {
			break;
		}
		matcher = matcher->next;
	}
	if (url) {
		delete url;
	}
	if (ss == NULL) {
		return false;
	}
	char *key = ss->getString(1);
	if (key == NULL) {
		delete ss;
		return false;
	}
	lock.Lock();
	std::map<char *, KRequestQueue *, lessp>::iterator it = queues.find(key);
	KRequestQueue *queue;
	if (it == queues.end()) {
		queue = new KRequestQueue;
		queue->set(max_worker, max_queue);
		queues.insert(std::pair<char *, KRequestQueue *>(strdup(key), queue));
	} else {
		queue = (*it).second;
	}
	rq->ctx->queue_handled = false;
	rq->queue = queue;
	queue->addRef();	
	lock.Unlock();
	addCallBack(rq, key);
	delete ss;
	return true;
}
void KPerQueueMark::editHtml(std::map<std::string, std::string> &attribute) 
{
	max_worker = atoi(attribute["max_worker"].c_str());
	max_queue = atoi(attribute["max_queue"].c_str());
	lock.Lock();
	std::map<char *, KRequestQueue *, lessp>::iterator it;
	for (it = queues.begin(); it != queues.end(); it++) {
		(*it).second->set(max_worker, max_queue);
	}
	lock.Unlock();
	while (this->matcher) {
		per_queue_matcher *next = this->matcher->next;
		delete this->matcher;
		this->matcher = next;
	}
	per_queue_matcher *last = NULL;
	char *url = strdup(attribute["url"].c_str());
	char *hot = url;
	while (hot) {
		per_queue_matcher *matcher = new per_queue_matcher;
		char *p = strchr(hot, PER_MATCHER_SPLIT_CHAR);
		if (p) {
			*p = '\0';
			p++;
		}
		char *val = hot;
		if (*hot == '#') {
			hot++;
			val = strchr(val, ':');
			if (val == NULL) {
				delete matcher;
				hot = p;
				continue;
			}
			*val = '\0';
			val++;
			matcher->header = strdup(hot);
		}
		matcher->reg.setModel(val, PCRE_CASELESS);
		if (last) {
			last->next = matcher;
		}
		last = matcher;
		if (this->matcher == NULL) {
			this->matcher = matcher;
		}
		hot = p;
	}
	free(url);

}
std::string KPerQueueMark::getDisplay()
{
	std::stringstream s;
	s << max_worker << " " << max_queue << "/";
	lock.Lock();
	s << queues.size();
	lock.Unlock();
	return s.str();
}
void KPerQueueMark::buildXML(std::stringstream &s)
{
	s << " url='";
	build_matcher(s);
	s << "' max_worker='" << max_worker << "' max_queue='" << max_queue << "'>";
}
void KPerQueueMark::build_matcher(std::stringstream &s)
{
	per_queue_matcher *matcher = this->matcher;
	while (matcher) {
		if (matcher->header) {
			s << "#" << matcher->header << ":";
		}
		s << matcher->reg.getModel();
		if (matcher->next) {
			s << PER_MATCHER_SPLIT_CHAR;
		}
		matcher = matcher->next;
	}
}
std::string KPerQueueMark::getHtml(KModel *model)
{
	KPerQueueMark *mark = (KPerQueueMark *)(model);
	std::stringstream s;
	s << "url:<input name='url' value='";
	if (mark) {
		mark->build_matcher(s);
	}
	s << "'>";
	s << "max_worker:<input name='max_worker' value='";
	if (mark) {
		s << mark->max_worker;
	}
	s << "'>";
	s << "max_queue:<input name='max_queue' value='";
	if (mark) {
		s << mark->max_queue;
	}
	s << "'>\n";
	return s.str();
}
void KPerQueueMark::callBack(char *key)
{
	std::map<char *, KRequestQueue *, lessp>::iterator it;
	lock.Lock();
	it = queues.find(key);
	if (it != queues.end() && (*it).second->getRef()==1) {
		free((*it).first);
		(*it).second->release();
		queues.erase(it);
	}
	lock.Unlock();
	KAccess::releaseRunTimeModel(this);
}
void per_queue_mark_call_back(void *data)
{
	per_queue_arg *param = (per_queue_arg *)data;
	param->mark->callBack(param->key);
	free(param->key);
	delete param;
}
#endif
