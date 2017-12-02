#include "KDiskCacheIndex.h"
#include "KHttpObject.h"
#ifdef ENABLE_DB_DISK_INDEX
KDiskCacheIndex *dci = NULL;
void loadDiskCacheIndex(const char *url,const char *data,int dataLen)
{
	if (dataLen!=sizeof(HttpObjectIndex)) {
		klog(KLOG_ERR,"dataLen = %d is not eq sizeof(HttpObjectIndex)=%d\n",dataLen,sizeof(HttpObjectIndex));
		return;
	}
	KHttpObject *obj = new KHttpObject;
	memcpy(&obj->index,data,dataLen);
	cor_result result = create_http_object(obj,url);
	if (result==cor_success) {
		dci->load_count++;
	}
	obj->release();
	return;
}
KTHREAD_FUNCTION diskCacheIndexCallBack(void *data,int msec)
{
	diskCacheOperatorParam *param = (diskCacheOperatorParam *)data;
	dci->work(param);
	delete param;
	KTHREAD_RETURN;
}
KDiskCacheIndex::KDiskCacheIndex()
{
	worker = new KAsyncWorker(1);
	load_count = 0;
}
KDiskCacheIndex::~KDiskCacheIndex()
{
	worker->release();
}
void KDiskCacheIndex::work(diskCacheOperatorParam *param)
{
	insertTranscation();
	bool result = false;
	switch (param->op) {
	case ci_add:
	{
		assert(param->url);
		result = add(param->filename1,param->filename2,param->url,kgl_current_sec,(char *)&param->data,sizeof(param->data));
		free(param->url);
		break;
	}
	case ci_update:
	{
		result = update(param->filename1,param->filename2,(char *)&param->data,sizeof(param->data));
		break;
	}
	case ci_del:
		result = del(param->filename1,param->filename2);
		break;
	case ci_updateLast:
		result = updateLast(param->filename1,param->filename2,kgl_current_sec);
		break;
	case ci_close:
		this->close();
		result = true;
		break;
	case ci_begin:
		result = this->begin();
		break;
	case ci_commit:
		result = this->commit();
		transaction = false;
		break;
	case ci_load:
		result = this->load(loadDiskCacheIndex);
		transaction = false;
		break;
	default:
		break;
	}
}
void KDiskCacheIndex::insertTranscation()
{
	if (transaction == true) {
		return;
	}
	this->begin();
	transaction = true;
	start(ci_commit,NULL);	
}
void KDiskCacheIndex::start(ci_operator op,KHttpObject *obj)
{
	diskCacheOperatorParam *param = new diskCacheOperatorParam;
	memset(param,0,sizeof(diskCacheOperatorParam));
	param->op = op;
	switch (op) {
	case ci_add:
		param->url = obj->url->getUrl();
	case ci_update:
		param->filename1 = obj->index.filename1;
		param->filename2 = obj->index.filename2;		
		memcpy(&param->data,&obj->index,sizeof(param->data));
		break;
	case ci_del:
	case ci_updateLast:
		param->filename1 = obj->index.filename1;
		param->filename2 = obj->index.filename2;			
		break;
	case ci_close:
		break;
	case ci_begin:
		transaction = true;
		break;
	case ci_commit:
		break;
	case ci_load:
		transaction = true;
		break;
	default:
		delete param;
		return;
	}
	//diskCacheIndexCallBack(param);
	worker->start(param,diskCacheIndexCallBack);
}
#endif
