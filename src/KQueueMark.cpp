#include "KQueueMark.h"
#ifdef ENABLE_REQUEST_QUEUE
#include "KRequestQueue.h"
#include "KAccess.h"
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
void KQueueMark::editHtml(std::map<std::string, std::string> &attribute) throw (KHtmlSupportException)
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
}
bool KPerQueueMark::mark(KHttpRequest *rq, KHttpObject *obj, const int chainJumpType, int &jumpType)
{
	if (rq->queue) {
		return false;
	}
	KStringBuf url;
	rq->url->getUrl(url);
	KRegSubString *ss = reg_url.matchSubString(url.getString(), url.getSize(), 0);
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
void KPerQueueMark::editHtml(std::map<std::string, std::string> &attribute) throw (KHtmlSupportException)
{
	max_worker = atoi(attribute["max_worker"].c_str());
	max_queue = atoi(attribute["max_queue"].c_str());
	lock.Lock();
	std::map<char *, KRequestQueue *, lessp>::iterator it;
	for (it = queues.begin(); it != queues.end(); it++) {
		(*it).second->set(max_worker, max_queue);
	}
	lock.Unlock();
	reg_url.setModel(attribute["url"].c_str(), PCRE_CASELESS);
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
	s << " url='" << reg_url.getModel() << "' max_worker='" << max_worker << "' max_queue='" << max_queue  << "'>";
}
std::string KPerQueueMark::getHtml(KModel *model)
{
	KPerQueueMark *mark = (KPerQueueMark *)(model);
	std::stringstream s;
	s << "url:<input name='url' value='";
	if (mark) {
		s << mark->reg_url.getModel();
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
void WINAPI per_queue_mark_call_back(void *data)
{
	per_queue_arg *param = (per_queue_arg *)data;
	param->mark->callBack(param->key);
	free(param->key);
	delete param;
}
#endif
