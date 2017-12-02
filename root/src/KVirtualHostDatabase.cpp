#include "KVirtualHostDatabase.h"
#include "KVirtualHostManage.h"
#include "KHttpServerParser.h"
#include "KTempleteVirtualHost.h"
#include "log.h"
#include "utils.h"
#include "server.h"
#include "KAcserverManager.h"
#include "malloc_debug.h"

KVirtualHostDatabase vhd;
static void set_vh_data(void *ctx,const char *name,const char *value)
{
	std::map<std::string,std::string> *attribute = (std::map<std::string,std::string> *)ctx;
	attribute->insert(std::pair<std::string,std::string>(name,value));
	return;
}
static const char *getx_vh_data(void *ctx,const char *name)
{
	std::map<std::string,std::string> *attribute = (std::map<std::string,std::string> *)ctx;
	std::map<std::string,std::string>::iterator it;
	it = attribute->find(name);
	if(it==attribute->end()){
		return NULL;
	}
	return (*it).second.c_str();
}
static const char *get_vh_data(void *ctx,const char *name)
{
	const char *value = getx_vh_data(ctx,name);
	if(value){
		return value;
	}
	return "";
}
void init_vh_data(vh_data *vd,std::map<std::string,std::string> *attribute)
{
	vd->set = set_vh_data;
	vd->get = get_vh_data;
	vd->getx = getx_vh_data;
	vd->ctx = (void *)attribute;
}
static const char *getSystemEnv(void *param,const char *name)
{
	static std::string value;
	const char *value2 = getSystemEnv(name);
	if(value2){
		return value2;
	}
	if(!conf.gvm->globalVh.getEnvValue(name,value)){
		return NULL;
	}
	return value.c_str();
}
KVirtualHostDatabase::KVirtualHostDatabase()
{
	ext = false;
	lastStatus = false;
	memset(&vhm,0,sizeof(vhm));
	vhm.vhi_version = 1;
	vhm.cbsize = sizeof(vhm);
	vhm.getConfigValue = getSystemEnv;
}
KVirtualHostDatabase::~KVirtualHostDatabase()
{
	clear();
}
bool KVirtualHostDatabase::	check()
{
	lock.Lock();
	if (vhm.createConnection == NULL) {
		lock.Unlock();
		return false;
	}
	void *cn = vhm.createConnection();
	lock.Unlock();
	bool result = false;
	if(cn){
		result = true;
		vhm.freeConnection(cn);
	}
	return result;
}
bool KVirtualHostDatabase::flushVirtualHost(const char *vhName,bool initEvent,KVirtualHostEvent *ctx)
{
	lock.Lock();
	if (vhm.createConnection == NULL) {
		ctx->setStatus("not load vh_database driver");
		lock.Unlock();
		return false;
	}
	void *cn = vhm.createConnection();
	if(cn==NULL){
		ctx->setStatus("cann't create connection");
		lock.Unlock();
		return false;
	}
	KVirtualHost *ov = conf.gvm->refsVirtualHostByName(vhName);
	KVirtualHost *vh = NULL;
	void *rs = vhm.flushVirtualHost(cn,vhName);
	
	//KVirtualHostData *rs = cn->flushVirtualHost(vhName);
	lock.Unlock();
	if(rs==NULL){
		ctx->setStatus("cann't load virtualHost");
		vhm.freeConnection(cn);
		//delete cn;
		if(ov){
			ov->destroy();
		}
		return false;
	}
	vh_data vd;
	std::map<std::string,std::string> attribute;
	init_vh_data(&vd,&attribute);
	if(vhm.query(rs,&vd)){
		vh = newVirtualHost(cn,attribute,conf.gvm,ov);
	} else if(ov) {
		conf.gvm->removeVirtualHost(ov);
	}
	vhm.freeStmt(rs);
	vhm.freeConnection(cn);
	if (vh && ctx) {
		//vh的引用由ctx处理了.
		ctx->buildVh(vh);
		KTempleteVirtualHost *tvh = vh->tvh;
		if (initEvent) {
			if (tvh) {
				tvh->initEvent(ctx);
			}
#ifndef HTTP_PROXY
			conf.gam->killAllProcess(vh);
#endif
		} else {
			if (tvh) {
				tvh->updateEvent(ctx);
			}
		}
	}
	if(ov){
		ov->destroy();
	}
	return true;
}
bool KVirtualHostDatabase::isLoad()
{
	return (vhm.createConnection != NULL);
}
bool KVirtualHostDatabase::loadVirtualHost(KVirtualHostManage *vm,std::string &errMsg)
{
	lock.Lock();
	lastStatus = false;
	if (vhm.createConnection == NULL) {
		errMsg = "not load vh_database driver";
		lock.Unlock();
		return false;
	}
	void *cn = vhm.createConnection();
	if(cn==NULL){
		errMsg = "cann't connect to database";
		lock.Unlock();
		return false;
	}
	lastStatus = true;
	void *rs = vhm.loadVirtualHost(cn);
	lastStatus = (rs!=NULL);
	lock.Unlock();
	if(rs==NULL){
		vhm.freeConnection(cn);
		return true;
	}
	vh_data vd;
	std::map<std::string,std::string> attribute;
	init_vh_data(&vd,&attribute);
	while(vhm.query(rs,&vd)){
		//防止加载时间太长，而安全进程误认为挂掉。
		setActive();
		KVirtualHost *vh = newVirtualHost(cn,attribute,vm,NULL);
		if(vh){
			vh->destroy();
		}
		attribute.clear();
	}
	vhm.freeStmt(rs);
	vhm.freeConnection(cn);
	return true;
}
bool KVirtualHostDatabase::parseAttribute(std::map<std::string,std::string> &attribute)
{
	bool result = false;
	lock.Lock();
	std::string driver = attribute["driver"];
	if (vhm.createConnection == NULL) {	
		if(!isAbsolutePath(driver.c_str())){
			driver = conf.path + driver;
		}
		if(!vhm_handle.isloaded()){
			if(!vhm_handle.load(driver.c_str())){
				lock.Unlock();
				klog(KLOG_ERR,"cann't load driver [%s]\n",driver.c_str());
				return false;
			}
		}
		initVirtualHostModulef m_init_vh_module = (initVirtualHostModulef)vhm_handle.findFunction("initVirtualHostModule");
		if(m_init_vh_module==NULL){
			lock.Unlock();
			klog(KLOG_ERR,"cann't find initVirtualHostModule function in driver [%s]\n",driver.c_str());
			return false;
		}
		if(m_init_vh_module(&vhm) == 0 || vhm.createConnection==NULL){
			lock.Unlock();
			klog(KLOG_ERR,"Cann't init vh module in driver [%s]\n",driver.c_str());
			return false;
		}
		ext = cur_config_ext;
	}
	if(vhm.parseConfig){
		vh_data vd;
		init_vh_data(&vd,&attribute);
		vhm.parseConfig(&vd);
	}
	result = true;
	lock.Unlock();
	std::string errMsg;
	if(!vhd.loadVirtualHost(conf.gvm,errMsg)){
		klog(KLOG_ERR,"load virtual host failed. %s\n",errMsg.c_str());
	}
	return result;
}
KVirtualHost *KVirtualHostDatabase::newVirtualHost(void *cn,std::map<std::string,std::string> &attribute,KVirtualHostManage *vm,KVirtualHost *ov)
{
	KTempleteVirtualHost *tm = NULL;
	KVirtualHost *vh = NULL;
	bool result = false;
	std::string templete = attribute["templete"];
	if(templete.size()>0){
		std::string subtemplete = attribute["subtemplete"];
		if(subtemplete.size()>0){
			templete += ":";
			templete += subtemplete;
		}	
		tm = vm->refsTempleteVirtualHost(templete);		
	}
#ifndef HTTP_PROXY
	vh = KHttpServerParser::buildVirtualHost(attribute,&vm->globalVh,tm,ov);
#endif
	if(vh){
		vh->db = true;
		vh->addRef();
		loadInfo(vh,cn);
		conf.gvm->inheritVirtualHost(vh,false);
		if (ov) {
			result = vm->updateVirtualHost(vh,ov);
		} else {
			result = vm->updateVirtualHost(vh);
		}
	}
	if(tm){
		tm->destroy();
	}
	if(!result && vh){		
		vh->destroy();
		return NULL;
	}
	return vh;
}
bool KVirtualHostDatabase::loadInfo(KVirtualHost *vh,void *cn)
{
	if(vhm.loadInfo == NULL){
		return false;
	}
	std::map<std::string,std::string> attribute;
	vh_data vd;
	init_vh_data(&vd,&attribute);
	void *rs = vhm.loadInfo(cn,vh->name.c_str());
	if(rs==NULL){
		return false;
	}
	while(vhm.query(rs,&vd)){
		const char *type = attribute["type"].c_str();
		const char *name = attribute["name"].c_str();
		const char *value = attribute["value"].c_str();
		if (attribute["skip_kangle"]=="1") {
			continue;
		}
		if(type==NULL || name==NULL){
			attribute.clear();
			continue;
		}
		int t = atoi(type);
		switch(t){
			case VH_INFO_HOST:
			case VH_INFO_HOST2:
			{
				KSubVirtualHost *svh = new KSubVirtualHost(vh);
				svh->setHost(name);
				svh->setDocRoot(vh->doc_root.c_str(),value);
				vh->hosts.push_front(svh);
				break;
			}
			case VH_INFO_ERROR_PAGE:
			{
				if(value){
					vh->addErrorPage(atoi(name),value);
				}
				break;
			}
			case VH_INFO_INDEX:
			{
				vh->addIndexFile(name,atoi(value));
				break;
			}
			case VH_INFO_ALIAS:
			{
				char *buf = strdup(value);
				char *to = buf;
				char *p = strchr(buf,',');
				if (p) {
					*p = '\0';
					p++;
					char *internal = p;
					p = strchr(internal,',');
					std::string errMsg;
					if (p) {
						*p = '\0';
						p++;
						vh->addAlias(name,to,vh->doc_root.c_str(),*internal=='1',atoi(p),errMsg);
					}
				}
				free(buf);
				break;
			}
			case VH_INFO_MAP:
			{
				//name格式       是否文件扩展名1|0,值
				//value格式      是否验证文件存在1|0,target,allowMethod
				const char *map_val = strchr(name,',');
				if (map_val) {
					map_val ++;
					bool file_ext = (*name=='1');
					char *buf = strdup(value);
					char *p = strchr(buf,',');
					if (p) {
						*p = '\0';
						p++;
						char *target = p;
						p = strchr(p,',');
						if (p) {
							*p = '\0';
							p++;
							char *allowMethod = p;
							bool confirmFile = false;
							if (*buf=='1') {
								confirmFile = true;
							}
							vh->addRedirect(file_ext,map_val,target,allowMethod,confirmFile,"");
						}
					}
					free(buf);
				}
				break;
			}
			case VH_INFO_MIME:
			{
				char *buf = strdup(value);
				char *p = strchr(buf,',');
				if(p){
					*p = '\0';
					p++;
					bool gzip = (*p=='1');
					p = strchr(p,',');
					if(p){
						int max_age = atoi(p+1);
						vh->addMimeType(name,buf,gzip,max_age);
					}
				}
				free(buf);
				break;
			}
#ifdef ENABLE_BASED_PORT_VH
			case VH_INFO_BIND:
			{
				vh->binds.push_back(name);
				break;
			}
#endif
			default:
				vh->addEnvValue(name,value);
		}
		attribute.clear();
	}
	vhm.freeStmt(rs);
	return true;
}
void KVirtualHostDatabase::clear()
{
	if(ext){
		return;
	}	
}
void KVirtualHostDatabase::freeConnection(void *cn)
{
	if (vhm.freeConnection) {
		vhm.freeConnection(cn);
	}
}
void *KVirtualHostDatabase::createConnection()
{
	lock.Lock();
	if(vhm.createConnection == NULL){
		lock.Unlock();
		return NULL;
	}
	void *cn = vhm.createConnection();
	lock.Unlock();
	return cn;
}
#if 0
bool KVirtualHostDatabase::addInfo(std::map<std::string,std::string> &attribute,std::string &errMsg,bool skipFlush)
{
	if (vhm.addInfo == NULL) {
		errMsg = "operator not support";
		return false;
	}
	void *cn = createConnection();
	if(cn==NULL){
		return false;
	}
	vh_data vd;
	init_vh_data(&vd,&attribute);
	int result = vhm.addInfo(cn,&vd);
	vhm.freeConnection(cn);
	if (skipFlush) {
		return result>0;
	}
	return flushVirtualHost(attribute["vhost"].c_str(),false,NULL);
}
bool KVirtualHostDatabase::delInfo(std::map<std::string,std::string> &attribute,std::string &errMsg,bool skipFlush)
{
	if (vhm.delInfo == NULL) {
		errMsg = "operator not support";
		return false;
	}
	void *cn = createConnection();
	if(cn==NULL){
		return false;
	}
	vh_data vd;
	init_vh_data(&vd,&attribute);
	int result = vhm.delInfo(cn,&vd);
	vhm.freeConnection(cn);
	if (skipFlush) {
		return result>0;
	}
	return flushVirtualHost(attribute["vhost"].c_str(),false,NULL);
}
bool KVirtualHostDatabase::addVirtualHost(
	std::map<std::string,std::string> &attr,
	KVirtualHostEvent *ctx,
	std::string &errMsg)
{
	if (vhm.addVirtualHost == NULL) {
		errMsg = "operator not support";
		return false;
	}
	std::string name = attr["name"];
	if (name.size() == 0) {
		errMsg = "vh name is empty";
		return false;
	}
#ifdef _WIN32
	//windows版本修正doc_root
	std::string doc_root = attr["doc_root"];
	if(doc_root.size()>0 && doc_root[0]=='/'){
		std::string dev;
		conf.gvm->globalVh.getEnvValue("dev",dev);
		attr["doc_root"] = dev + doc_root;
	}
#endif
	void *cn = createConnection();
	if(cn==NULL){
		return false;
	}
	vh_data vd;
	init_vh_data(&vd,&attr);
	vhm.addVirtualHost(cn,&vd);
	vhm.freeConnection(cn);
	return flushVirtualHost(name.c_str(),attr["init"]=="1",ctx);
}
bool KVirtualHostDatabase::delVirtualHost(std::map<std::string,std::string> &attr)
{	
	if (vhm.delVirtualHost == NULL) {
		//errMsg = "operator not support";
		return false;
	}
	void *cn = createConnection();
	if(cn==NULL){
		return false;
	}
	vh_data vd;
	init_vh_data(&vd,&attr);
	vhm.delVirtualHost(cn,&vd);
	vhm.freeConnection(cn);
	return true;
}
bool KVirtualHostDatabase::updateVirtualHost(KVirtualHostEvent *ctx,std::map<std::string,std::string> &attribute,std::string &errMsg)
{
	if (vhm.updateVirtualHost == NULL) {
		errMsg = "operator not support";
		return false;
	}
	void *cn = createConnection();
	if(cn==NULL){
		return false;
	}
	vh_data vd;
	init_vh_data(&vd,&attribute);
	vhm.updateVirtualHost(cn,&vd);
	vhm.freeConnection(cn);
	return flushVirtualHost(attribute["name"].c_str(),attribute["init"]=="1",ctx);
}
bool KVirtualHostDatabase::saveFlow(KVirtualHost *vh,void *cn)
{	
	if (vhm.setFlow) {
		std::map<std::string,std::string> attribute;
		std::stringstream s;
		if (vh->flow==NULL) {
			return false;
		}
		s << vh->flow->flow;
		attribute["flow"] = s.str();
		s.str("");
		s << vh->flow->cache;
		attribute["hcount"] = s.str();
		attribute["name"] = vh->name;
		vh_data vd;
		init_vh_data(&vd,&attribute);
		int result = vhm.setFlow(cn,&vd)>0;
		if (result) {
			vh->flow->flow = 0;
			vh->flow->cache = 0;
			return true;
		}
	}
	return false;
}
#endif
