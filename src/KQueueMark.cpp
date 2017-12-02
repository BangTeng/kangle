#include "KQueueMark.h"
#ifdef ENABLE_REQUEST_QUEUE
#include "KRequestQueue.h"
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
	if (mark && queue) {
		s << mark->queue->getMaxQueue();
	}
	s << "'>\n";

	return s.str();
}
#endif
