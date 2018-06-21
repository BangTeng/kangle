/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 *
 * kangle web server              http://www.kangleweb.net/
 * ---------------------------------------------------------------------
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *  See COPYING file for detail.
 *
 *  Author: KangHongjiu <keengo99@gmail.com>
 */
#include <map>
#include <assert.h>
#include <string>
#include <sstream>

#include "do_config.h"
#include "utils.h"
#include "log.h"
#include "md5.h"
#include "lib.h"
#include "lang.h"
#include "KConfigBuilder.h"
#include "KConfigParser.h"
#include "KModelManager.h"
#include "KAccess.h"
#include "KLang.h"
#include "KHttpServerParser.h"
#include "KContentType.h"
#include "malloc_debug.h"
#include "KVirtualHostManage.h"
#include "KVirtualHostDatabase.h"
#include "KAcserverManager.h"
#include "KWriteBackManager.h"
#include "KSelectorManager.h"
#include "KListenConfigParser.h"
#include "directory.h"
#include "server.h"
#include "KHtAccess.h"
#include "KLogHandle.h"
#include "cache.h"
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/mman.h>
#endif
bool need_reboot_flag = false;
using namespace std;
KConfig *cconf = NULL;
KGlobalConfig conf;
KConfigBase::KConfigBase()
{
	memset(this,0,sizeof(KConfigBase));
		
}
void KConfig::copy(KConfig *c)
{
	//把cconf赋值到conf中
	KConfigBase *bc = static_cast<KConfigBase *>(this);
	memcpy(bc,static_cast<KConfigBase *>(c),sizeof(KConfigBase));
	conf.admin_lock.Lock();
	this->admin_ips.swap(c->admin_ips);
	this->admin_user = c->admin_user;
	this->admin_passwd = c->admin_passwd;
	this->service.swap(c->service);
	conf.admin_lock.Unlock();
	this->ssl_client_chiper = c->ssl_client_chiper;
	this->ssl_client_protocols = c->ssl_client_protocols;
	this->run_user = c->run_user;
	this->run_group = c->run_group;
	
	ipLock.Lock();
	//swap per_ip_head
	KPerIpConnect *tp = per_ip_head;
	per_ip_head = c->per_ip_head;
	c->per_ip_head = tp;
	//swap per_ip_last
	tp = per_ip_last;
	per_ip_last = c->per_ip_last;
	c->per_ip_last = tp;
	ipLock.Unlock();
	return;
}
KConfig::~KConfig()
{
	//todo:清除内存
	std::vector<KListenHost *>::iterator it;
	for(it=service.begin();it!=service.end();it++){
		delete (*it);
	}
	while (per_ip_head) {
		per_ip_last = per_ip_head->next;
		delete per_ip_head;
		per_ip_head = per_ip_last;
	}

}
KGlobalConfig::KGlobalConfig()
{
	gam = new KAcserverManager;
	gvm = new KVirtualHostManage;
	sysHost = new KVirtualHost;
	hfdm = NULL;
}
class KExtConfigDynamicString : public KDynamicString
{
public:
	KExtConfigDynamicString(const char *file)
	{
		this->file = file;
		path = getPath(file);
	}
	~KExtConfigDynamicString()
	{
		if(path){
			xfree(path);
		}
	}
	const char *getValue(const char *name)
	{
		if(strcasecmp(name,"config_dir")==0){
			return path;
		}
		if(strcasecmp(name,"config_file")==0){
			return file;
		}
		return NULL;
	}
private:
	char *path;
	const char *file;
};
class KExtConfig
{
public:
	KExtConfig(char *content)
	{
		this->content = content;
		next = NULL;
	}
	~KExtConfig()
	{
		if(content){
			xfree(content);
		}
		if(next){
			delete next;
		}
	}
	std::string file;
	char *content;
	bool merge;
	KExtConfig *next;
};
void KGlobalConfig::set_autoupdate_time(int autoupdate_time)
{
	if(autoupdate_time < 0){
		autoupdate_time = 0;
	}
	if(autoupdate_time > 23){
		autoupdate_time = 23;
	}
	stringstream s;
	s << "0 " << autoupdate_time << " * * *";
	if(!autoupdate_install_time.set(s.str().c_str())){
		klog(KLOG_ERR,"cann't set autoupdate install time [%s]\n",s.str().c_str());
	}
	this->autoupdate_time = autoupdate_time;
}
static map<int,KExtConfig *> extconfigs;
bool get_size_radio(INT64 size,int radio,const char radio_char,std::stringstream &s)
{
	INT64 t;
	t = size>>radio;
	if (t>0) {
		if((t<<radio) == size){
			s << t << radio_char;
			return true;
		}
	}
	return false;
}
std::string get_size(INT64 size)
{
	std::stringstream s;
	if(get_size_radio(size,40,'T',s)){
		return s.str();
	}
	if(get_size_radio(size,30,'G',s)){
		return s.str();
	}
	if(get_size_radio(size,20,'M',s)){
		return s.str();
	}
	if(get_size_radio(size,10,'K',s)){
		return s.str();
	}
	s << size;
	return s.str();
}
INT64 get_radio_size(const char *size,bool &is_radio) {
	is_radio = false;
	INT64 cache_size = string2int(size);
	int len = strlen(size);
	char t = 0;
	for (int i = 0; i < len; i++) {
		if (!(size[i] >= '0' && size[i] <= '9')) {
			t = size[i];
			break;
		}
	}
	switch (t) {
	case 'k':
	case 'K':
		return cache_size << 10;
	case 'm':
	case 'M':
		return cache_size << 20;
	case 'g':
	case 'G':
		return cache_size << 30;
	case 't':
	case 'T':
		return cache_size << 40;
	case '%':
		is_radio = true;
		if (cache_size<=0) {
			cache_size = 0;
		}
		if (cache_size >=100) {
			cache_size = 100;
		}
		break;
	}
	return cache_size;
}
INT64 get_size(const char *size) {
	bool is_radio = false;
	return get_radio_size(size,is_radio);
}
void init_config(KConfig *conf)
{
#ifdef MALLOCDEBUG
	conf->mallocdebug = true;
#endif
	conf->time_out = 60;
	conf->keep_alive_count = 0;
	conf->max = 500;
	conf->refresh = REFRESH_AUTO;
	conf->refresh = 10;
	conf->log_level = 2;
	conf->path_info = true;
	conf->passwd_crypt = CRYPT_TYPE_PLAIN;

	SAFE_STRCPY(conf->access_log ,"access.log");
	conf->maxLogHandle = 2;
	conf->autoupdate = AUTOUPDATE_ON;
#ifdef ENABLE_TF_EXCHANGE
	conf->max_post_size = 8388608;
#endif	
	conf->read_hup = true;
	conf->mlock = false;
	conf->io_timeout = 4;
	conf->max_io = 0;
	conf->worker_io = 16;
	conf->worker_dns = 32;
	conf->io_buffer = 262144;
}
void LoadDefaultConfig() {
	init_config(&conf);
	conf.autoupdate = AUTOUPDATE_ON;
	conf.set_autoupdate_time(3);
	conf.ioWorker = NULL;
	conf.dnsWorker = NULL;
#ifdef ENABLE_DISK_CACHE
	conf.diskWorkTime.set(NULL);
#endif
#ifdef KANGLE_WEBADMIN_DIR
	conf.sysHost->doc_root = KANGLE_WEBADMIN_DIR;
#else
	conf.sysHost->doc_root = conf.path;
	conf.sysHost->doc_root += "webadmin";
#endif
	conf.sysHost->browse = false;
	KSubVirtualHost *svh = new KSubVirtualHost(conf.sysHost);
	svh->setDocRoot(conf.sysHost->doc_root.c_str(), "/");
	
	conf.sysHost->hosts.push_back(svh);
	conf.sysHost->addRef();
}
void loadExtConfigFile(KExtConfig *config,KXml &xmlParser)
{
	while(config){
		if (config->merge) {
			cur_config_ext = false;
		} else {
			cur_config_ext = true;
		}
		klog(KLOG_NOTICE,"load config file [%s]\n",config->file.c_str());
		try{
			xmlParser.startParse(config->content);
		}catch(KXmlException &e){
			fprintf(stderr, "%s in file[%s]\n", e.what(),config->file.c_str());
		}
		config = config->next;
	}
}
bool load_config_file(KFileName *file,int inclevel,KStringBuf &s,int &id,bool &merge)
{
	if(inclevel > 128){
		klog(KLOG_ERR,"include level [%d] is limited.\n",inclevel);
		return false;
	}
	int len = (int)file->fileSize;
	if(len<=0 || len>1048576){
		klog(KLOG_ERR,"config file [%s] length is wrong\n",file->getName());
		return false;
	}
	KFile fp;
	if (!fp.open(file->getName(),fileRead)) {
		klog(KLOG_ERR,"cann't open file[%s]\n",file->getName());
		return false;
	}
	char *buf = (char *)xmalloc(len+1);
	int read_len = fp.read(buf,len);
	if(read_len!=len){
		klog(KLOG_ERR,"this sure not be happen,read file [%s] size error.\n",file->getName());
		xfree(buf);
		return false;
	}
	buf[len] = '\0';
	KExtConfigDynamicString ds(file->getName());
	ds.dimModel = false;
	ds.blockModel = false;
	ds.envChar = '%';
	char *content = ds.parseDirect(buf);
	xfree(buf);
	char *hot = content;
	while(*hot && isspace((unsigned char)*hot)){
		hot++;
	}
	char *start = hot;
	//默认启动顺序为50
	id = 50;
	if(strncmp(hot,"<!--#",5)==0){
		hot+=5;
		if (strncmp(hot,"stop",4)==0) {
			/*
			 * 扩展没有启动
			 */
			xfree(content);
			return false;
		} else if (strncmp(hot,"start",5)==0) {
			char *end = strchr(hot,'>');
			if (end) {
				start = end+1;
			}
			hot+=6;
			id = atoi(hot);
			char *p = strchr(hot,' ');
			if (p) {
				if (inclevel>0 && strncmp(p+1,"merge",5)==0) {
					merge = true;
					conf.mergeFiles.push_back(file->getName());
				}
			}
		}
	}
	klog(KLOG_NOTICE,"read config file [%s] success\n",file->getName());
	hot = start;
	for (;;) {
		char *p = strstr(hot,"<!--#include");
		if (p==NULL) {
			s << hot;
			break;
		}
		int pre_hot_len = p - hot;
		p += 12;
		s.write_all(hot,pre_hot_len);
		hot = strstr(p,"-->");
		if(hot==NULL){
			break;
		}
		while(p<hot && isspace((unsigned char)*p)){
			p++;
		}
		int filelen = hot-p;
		if(filelen<=0){
			break;
		}
		char *incfilename = (char *)xmalloc(filelen+1);
		memcpy(incfilename,p,filelen);
		incfilename[filelen] = '\0';
		for(int i=filelen-1;i>0;i--){
			if(!isspace((unsigned char)incfilename[i])){
				break;
			}
			incfilename[i] = '\0';
		}
		bool incresult = false;
		char *translate_filename = ds.parseDirect(incfilename);
		xfree(incfilename);
		KFileName incfile;
		if (translate_filename) {				
			if (!isAbsolutePath(translate_filename)) {
				incresult = incfile.setName(conf.path.c_str(),translate_filename,FOLLOW_LINK_ALL);
			} else {
				incresult = incfile.setName(translate_filename);
			}
			xfree(translate_filename);
		}		
		if (incresult) {
			int id;
			bool merge=false;
			load_config_file(&incfile,inclevel+1,s,id,merge);
		}
		hot += 3;
	}
	xfree(content);
	return true;
}
void loadExtConfigFile(KFileName *file) {
	KStringBuf s;
	int id = 50;
	bool merge = false;
	if (!load_config_file(file,0,s,id,merge)) {
		return;
	}
	KExtConfig *extconf = new KExtConfig(s.stealString());
	extconf->file = file->getName();
	extconf->merge = merge;
	map<int,KExtConfig *>::iterator it;
	it = extconfigs.find(id);
	if(it!=extconfigs.end()){
		extconf->next = (*it).second->next;
		(*it).second->next = extconf;
	}else{
		extconfigs.insert(pair<int,KExtConfig *>(id,extconf));
	}
}
int handleExtConfigFile(const char *file, void *param) {
	KFileName configFile;
	if (!configFile.setName((char *) param, file,FOLLOW_LINK_ALL)) {
		return 0;
	}
	if (configFile.isDirectory()) {
		KFileName dirConfigFile;
		if (!dirConfigFile.setName(configFile.getName(), "config.xml",
				FOLLOW_LINK_ALL)) {
			return 0;
		}
		loadExtConfigFile(&dirConfigFile);
		return 0;
	}
	loadExtConfigFile(&configFile);
	return 0;
}
void loadExtConfigFile()
{
#ifdef KANGLE_EXT_DIR
	std::string ext_path = KANGLE_EXT_DIR;
#else
	std::string ext_path = conf.path + "/ext";
#endif
	list_dir(ext_path.c_str(),handleExtConfigFile,(void *)ext_path.c_str());
	string configFile = conf.path + "etc/vh.d/";
	list_dir(configFile.c_str(),handleExtConfigFile,(void *)configFile.c_str());
}
bool saveConfig() {
	return KConfigBuilder::saveConfig();
}
void load_main_config(KConfig *cconf,KXml &xmlParser,bool firstload)
{
#ifdef KANGLE_ETC_DIR
	string configFile = KANGLE_ETC_DIR;
#else
	string configFile = conf.path + "/etc";
#endif
	configFile += CONFIG_FILE;
	printf(klang["LANG_READ_CONFIG_FILE"], configFile.c_str());
	
	for (int i=0;i<2;i++) {
		try {
			xmlParser.parseFile(configFile);
			break;
		} catch (KXmlException &e) {
			fprintf(stderr, "%s\n", e.what());
			if (i>0) {
				exit(0);
			} else {
				printf("cann't read config.xml try to read config.xml.lst\n");
				configFile = configFile + ".lst";
			}
		}
	}
	configFile = conf.path;
	configFile = configFile + "/etc/mime.types.xml";
	try {
		contentType.load(configFile);
	} catch (KXmlException &e) {
		fprintf(stderr, "%s\n", e.what());
	}
	try {
#ifdef KANGLE_WEBADMIN_DIR
		configFile = KANGLE_WEBADMIN_DIR;
#else
		configFile = conf.path + "/webadmin";
#endif
		configFile += "/lang.xml";
		klang.load(configFile.c_str());
	} catch (KXmlException &e) {
		fprintf(stderr, "%s\n", e.what());
	}
#ifndef HTTP_PROXY
	//废弃
	configFile = conf.path;
	configFile += VH_CONFIG_FILE;
	cur_config_vh_db = false;
	try {
		xmlParser.parseFile(configFile);
	} catch (...) {
	}
	if (vhd.isLoad()) {
		string errMsg;
		if (!vhd.loadVirtualHost(cconf->vm,errMsg)) {
			klog(KLOG_ERR, "Cann't load VirtualHost[%s]\n", errMsg.c_str());
			//db server failed.then skip delete exsit vh
			//lock.Unlock();
			cur_config_vh_db = false;
			return;
		}
	}
	cur_config_vh_db = false;
#endif
}
void clean_config() {
	kaccess[REQUEST].destroy();
	kaccess[RESPONSE].destroy();
	conf.admin_ips.clear();
	for (size_t i = 0; i < conf.service.size(); i++) {
		delete conf.service[i];
	}
	conf.service.clear();
#ifdef ENABLE_WRITE_BACK
	writeBackManager.destroy();
#endif
	contentType.destroy();
}
void do_config(bool firstTime) {
	cur_config_ext = false;
	if (firstTime) {
#ifdef _WIN32
		_setmaxstdio(2048);
#endif	
		for (int i=0;i<2;i++) {
			kaccess[i].setType(i);
			kaccess[i].setGlobal(true);
		}
		KAccess::loadModel();
	} else {
		vhd.clear();
	}
	assert(cconf==NULL);
	cconf = new KConfig;
	init_config(cconf);
	load_config(cconf,firstTime);
	conf.copy(cconf);
	if (!firstTime) {
		parse_config(false);
	}
	delete cconf;
	cconf = NULL;
#ifdef MCL_FUTURE
	if (firstTime && conf.mlock) {
		if (0 != mlockall(MCL_CURRENT | MCL_FUTURE)) {
			klog(KLOG_ERR, "Unable to mlockall\n");
		} else {
			klog(KLOG_NOTICE, "mlockall success\n");
		}
	}
#endif
}
void do_config_clean() {

}
int merge_apache_config(const char *filename)
{
	KFileName file;
	if(file.setName(filename)){
		KApacheConfig apConfig(false);
		std::stringstream s;
		s << "<!--#start 1000 merge-->\n";
		apConfig.load(&file,s);
		std::stringstream xf;
		xf << conf.path << PATH_SPLIT_CHAR << "ext";
		mkdir(xf.str().c_str(),0755);
		xf << "/_apache.xml";
		FILE *fp = fopen(xf.str().c_str(),"wt");
		if(fp==NULL){
			fprintf(stderr,"cann't open file [%s] to write\n",xf.str().c_str());
			return 1;
		}
		fwrite(s.str().c_str(),1,s.str().size(),fp);
		fclose(fp);
		fprintf(stdout,"success convert to file [%s] please reboot kangle\n",xf.str().c_str());
		return 0;
	} else {
		fprintf(stderr,"cann't open apache config file [%s]\n",filename);
		return 1;
	}
}
//读取配置文件，可重入
void load_config(KConfig *cconf,bool firstTime)
{
	std::map<int,KExtConfig *>::iterator it;
	bool main_config_loaded = false;
	loadExtConfigFile();
	KConfigParser parser;
	KAccess access[2];
	KAcserverManager am;
	KWriteBackManager wm;
	KVirtualHostManage vm;
	KXml xmlParser;
	xmlParser.setData(cconf);
	xmlParser.addEvent(&parser);
	xmlParser.addEvent(&listenConfigParser);
#ifndef HTTP_PROXY
	KHttpServerParser vhParser;
	xmlParser.addEvent(&vhParser);
#else
	xmlParser.addEvent(&kaccess[0]);
	xmlParser.addEvent(&kaccess[1]);
#endif
	if (firstTime) {
		xmlParser.addEvent(conf.gam);
#ifdef ENABLE_WRITE_BACK
		xmlParser.addEvent(&writeBackManager);
#endif
		cconf->am = conf.gam;
		cconf->vm = conf.gvm;
	} else {		
		for (int i=0;i<2;i++) {
			access[i].setType(i);
			access[i].setGlobal(true);
		}
#ifndef HTTP_PROXY
		vhParser.kaccess[0] = &access[0];
		vhParser.kaccess[1] = &access[1];
#endif
		xmlParser.addEvent(&am);
		xmlParser.addEvent(&wm);
		cconf->am = &am;
		cconf->vm = &vm;
	}

	for (it=extconfigs.begin();it!=extconfigs.end();it++) {
		if(!main_config_loaded && (*it).first>=100){
			main_config_loaded = true;
			cur_config_ext = false;
			load_main_config(cconf,xmlParser,firstTime);
			cur_config_ext = true;
		}
		loadExtConfigFile((*it).second,xmlParser);
		delete (*it).second;
	}
	extconfigs.clear();	
	cur_config_ext = false;
	if(!main_config_loaded){
		load_main_config(cconf,xmlParser,firstTime);
	}
	
	if (!firstTime) {
		conf.gam->copy(am);
		writeBackManager.copy(wm);
		conf.gvm->copy(&vm);
		for (int i=0;i<2;i++) {
			//access[i].setChainAction();
			kaccess[i].copy(access[i]);
		}
	} else {
		//for (int i = 0; i < 2; i++) {
		//	kaccess[i].setChainAction();
		//}
	}
	// load serial
	string serial_file = conf.path;
	serial_file += ".autoupdate.conf";
	FILE *fp = fopen(serial_file.c_str(),"rt");
	if(fp){
		fscanf(fp,"%d",&serial);
		fclose(fp);
	}
	cur_config_ext = false;
}
//解析配置文件,调用完load_config之后，对一些项目，解析处理，实施
void parse_config(bool firstTime)
{
	if (conf.worker_io<2) {
		conf.worker_io = 2;
	}
	if (conf.worker_dns<2) {
		conf.worker_dns = 2;
	}
	if (conf.ioWorker==NULL) {
		conf.ioWorker = new KAsyncWorker(conf.worker_io,conf.max_io);
	} else {
		conf.ioWorker->setWorker(conf.worker_io,conf.max_io);
	}
	if (conf.dnsWorker==NULL) {
		conf.dnsWorker = new KAsyncWorker(conf.worker_dns,512);
	} else {
		conf.dnsWorker->setWorker(conf.worker_dns,512);
	}
	if (*conf.disk_work_time) {
		conf.diskWorkTime.set(conf.disk_work_time);
	} else {
		conf.diskWorkTime.set(NULL);
	}
	if (firstTime) {
		//第一次才生效
		//生成serverName
		if (*conf.server_software) {
			SAFE_STRCPY(conf.serverName ,conf.server_software);
		} else {
			std::string serverName = PROGRAM_NAME;
			serverName += "/";
			serverName += VERSION;
			SAFE_STRCPY(conf.serverName,serverName.c_str());
		}
		conf.serverNameLength = strlen(conf.serverName);
		//生成disk_cache_dir
		if (*conf.disk_cache_dir2) {
			string disk_cache_dir = conf.disk_cache_dir2;
			pathEnd(disk_cache_dir);
			SAFE_STRCPY(conf.disk_cache_dir,disk_cache_dir.c_str());
		}
	}
	if (!firstTime) {
		//不是第一次，处理listen
		conf.gvm->flush_static_listens(conf.service);
		//重置log
		klog_start();
		selectorManager.setTimeOut();
	}
	cache.init(firstTime);
	
	::logHandle.setLogHandle(conf.logHandle);
	//BOOL result = SetProcessWorkingSetSize(GetCurrentProcess(),-2,-2);
}
