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
#include <string>
#include "WhmContext.h"

#include "whm.h"
#include "global.h"

#include "malloc_debug.h"
#include "KVirtualHostManage.h"
#include "KVirtualHostDatabase.h"
#include "KHttpManage.h"
#include "extern.h"
#include "server.h"
#include "cache.h"
#include "lib.h"
#include "KConfigBuilder.h"
#include "KAcserverManager.h"
#include "KTempleteVirtualHost.h"
#include "KHttpServerParser.h"
#include "KSelectorManager.h"
#include "KCdnContainer.h"
#include "ssl_utils.h"
#include "KAddr.h"
#include "KLogDrill.h"
enum{
	CALL_UNKNOW,
	CALL_INFO,
	CALL_ADD_VH,
	CALL_DEL_VH,
	CALL_EDIT_VH,
	CALL_INFO_VH,
	CALL_LIST_VH,
	CALL_LIST_TVH,
	CALL_LIST_GTVH,
	CALL_ADD_VH_INFO,
	CALL_DEL_VH_INFO,
	CALL_ADD_INDEX,
	CALL_DEL_INDEX,
	CALL_LIST_INDEX,
	CALL_ADD_REDIRECT,
	CALL_DEL_REDIRECT,
	CALL_ADD_ERROR_PAGE,
	CALL_DEL_ERROR_PAGE,
	CALL_SAVE_CONFIG,
	CALL_SAVE_VH,
	CALL_EXPORT_VH,
	CALL_EXPORT_CONFIG,
	CALL_ADD_SERVER,
	CALL_DEL_SERVER,
	CALL_RELOAD,
	CALL_RELOAD_VH,
	CALL_CHANGE_ADMIN_PASSWORD,
	CALL_KILL_PROCESS,
	CALL_WRITE_FILE,
	CALL_REBOOT,
	CALL_CHECK_VH_DB,
	CALL_UPDATE_VH,
	CALL_INFO_DOMAIN,

	CALL_ADD_TABLE,
	CALL_EMPTY_TABLE,
	CALL_DEL_TABLE,
	CALL_LIST_TABLE,
	
	CALL_LIST_CHAIN,
	CALL_ADD_CHAIN,
	CALL_EDIT_CHAIN,
	CALL_DEL_CHAIN,
	
	CALL_CLEAN_CACHE,
	CALL_CACHE_INFO,
	CALL_CACHE_PREFETCH,
	CALL_CLEAN_ALL_CACHE,

	CALL_GET_LOAD,
	CALL_DUMP_FLOW,
	CALL_DUMP_LOAD,
	CALL_GET_CONNECTION,
	CALL_PORT_MAP,
	CALL_CHECK_SSL,
	CALL_RUNTIME_MODEL,
	CALL_VH_STAT,
	CALL_BLACK_LIST,
	CALL_REPORT_IP,
	CALL_SERVER_INFO,
	CALL_QUERY_DOMAIN,
	CALL_LOG_DRILL
};
using namespace std;
BOOL WINAPI GetWhmVersion(WHM_VERSION_INFO *pVer) {
	return TRUE;
}
static int parseCallName(const char *callName)
{
	if(callName==NULL){
		return CALL_UNKNOW;
	}
	switch(*callName){
		case 'a':
			if(strcmp(callName,"add_vh_info")==0){
				return CALL_ADD_VH_INFO;
			}		
			if(strcmp(callName,"add_vh")==0){
				return CALL_ADD_VH;
			}
			if(strcmp(callName,"add_redirect")==0){
				return CALL_ADD_REDIRECT;
			}
			if(strcmp(callName,"add_error_page")==0){
				return CALL_ADD_ERROR_PAGE;
			}
			if(strcmp(callName,"add_server")==0){
				return CALL_ADD_SERVER;
			}
			if(strcmp(callName,"add_index")==0){
				return CALL_ADD_INDEX;
			}
			if(strcmp(callName,"add_table")==0){
				return CALL_ADD_TABLE;
			}
			if(strcmp(callName,"add_chain")==0){
				return CALL_ADD_CHAIN;
			}
			break;
		case 'b':
			if (strcmp(callName,"black_list")==0) {
				return CALL_BLACK_LIST;
			}
			break;
		case 'c':
			if(strcmp(callName,"change_admin_password")==0){
				return CALL_CHANGE_ADMIN_PASSWORD;
			}
			if(strcmp(callName,"check_vh_db")==0){
				return CALL_CHECK_VH_DB;
			}
			if(strcmp(callName,"clean_cache")==0){
				return CALL_CLEAN_CACHE;
			}
			if (strcmp(callName,"check_ssl")==0) {
				return CALL_CHECK_SSL;
			}
			if (strcmp(callName,"cache_info")==0) {
				return CALL_CACHE_INFO;
			}
			if (strcmp(callName,"cache_prefetch")==0) {
				return CALL_CACHE_PREFETCH;
			}
			if (strcmp(callName, "clean_all_cache") == 0) {
				return CALL_CLEAN_ALL_CACHE;
			}
			break;
		case 'd':
			if (strcmp(callName,"del_vh_info") == 0) {
				return CALL_DEL_VH_INFO;
			}
			if(strcmp(callName,"del_vh")==0){
				return CALL_DEL_VH;
			}
			if(strcmp(callName,"del_redirect")==0){
				return CALL_DEL_REDIRECT;
			}
			if(strcmp(callName,"del_error_page")==0){
				return CALL_DEL_ERROR_PAGE;
			}
			if(strcmp(callName,"del_server")==0){
				return CALL_DEL_SERVER;
			}
			if(strcmp(callName,"del_index")==0){
				return CALL_DEL_INDEX;
			}
			if(strcmp(callName,"del_table")==0){
				return CALL_DEL_TABLE;
			}
			if(strcmp(callName,"del_chain")==0){
				return CALL_DEL_CHAIN;
			}
#ifdef ENABLE_VH_FLOW
			if (strcmp(callName,"dump_flow")==0) {
				return CALL_DUMP_FLOW;
			}
#endif
			if (strcmp(callName,"dump_load")==0) {
				return CALL_DUMP_LOAD;
			}
			break;
		case 'e':
			if(strcmp(callName,"edit_vh")==0){
				return CALL_EDIT_VH;
			}
			if(strcmp(callName,"export_vh")==0){
				return CALL_EXPORT_VH;
			}
			if(strcmp(callName,"export_config")==0){
				return CALL_EXPORT_CONFIG;
			}
			if(strcmp(callName,"empty_table")==0){
				return CALL_EMPTY_TABLE;
			}
			if (strcmp(callName,"edit_chain")==0) {
				return CALL_EDIT_CHAIN;
			}
			break;
		case 'g':
#ifdef ENABLE_VH_FLOW
			if(strcmp(callName,"get_load")==0){
				return CALL_GET_LOAD;
			}
			if (strcmp(callName,"get_connection")==0) {
				return CALL_GET_CONNECTION;
			}

#endif
			break;
		case 'i':
			if(strcmp(callName,"info")==0){
				return CALL_INFO;
			}
			if(strcmp(callName,"info_vh")==0){
				return CALL_INFO_VH;
			}
			if(strcmp(callName,"info_domain")==0){
				return CALL_INFO_DOMAIN;
			}
			break;	
		case 'k':
			if(strcmp(callName,"kill_process")==0){
				return CALL_KILL_PROCESS;
			}
			break;
		case 'l':
			if(strcmp(callName,"list_vh")==0){
				return CALL_LIST_VH;
			}
			if(strcmp(callName,"list_tvh")==0){
				return CALL_LIST_TVH;
			}
			if(strcmp(callName,"list_gtvh")==0){
				return CALL_LIST_GTVH;
			}
			if(strcmp(callName,"list_table")==0){
				return CALL_LIST_TABLE;
			}
			if(strcmp(callName,"list_chain")==0){
				return CALL_LIST_CHAIN;
			}
			if (strcmp(callName,"list_index")==0) {
				return CALL_LIST_INDEX;
			}
#ifdef ENABLE_LOG_DRILL
			if (strcmp(callName, "log_drill") == 0) {
				return CALL_LOG_DRILL;
			}
#endif
			break;
		case 'p':
			if(strcmp(callName,"port_map")==0){
				return CALL_PORT_MAP;
			}
		case 'q':
			if (strcmp(callName, "query_domain") == 0) {
				return CALL_QUERY_DOMAIN;
			}
		case 'r':
			if(strcmp(callName,"reload_vh")==0){
				return CALL_RELOAD_VH;
			}
			if (strcmp(callName,"reboot")==0) {
				return CALL_REBOOT;
			}
			if (strcmp(callName,"reload")==0) {
				return CALL_RELOAD;
			}
			if (strcmp(callName,"runtime_model")==0) {
				return CALL_RUNTIME_MODEL;
			}
			if (strcmp(callName,"report_ip")==0) {
				return CALL_REPORT_IP;
			}
			break;
		case 's':
			if(strcmp(callName,"save_config")==0){
				return CALL_SAVE_CONFIG;
			}
			if(strcmp(callName,"save_vh")==0){
				return CALL_SAVE_VH;
			}
			if (strcmp(callName,"stat_vh")==0) {
				return CALL_VH_STAT;
			}
			if (strcmp(callName, "server_info") == 0) {
				return CALL_SERVER_INFO;
			}
			break;
		case 'u':
			if (strcmp(callName,"update_vh")==0) {
				return CALL_UPDATE_VH;
			}
			break;	
		case 'w':
			if(strcmp(callName,"write_file")==0){
				return CALL_WRITE_FILE;
			}
			break;
	}
	return CALL_UNKNOW;
}
#if 0
static int deleteVh(WhmContext *ctx)
{
	//TODO:���´���Ҫ����vhd.delVirtualHost��
	KUrlValue *uv = ctx->getUrlValue();
	string name = uv->get("name");
	KVirtualHost *vh = conf.gvm->refsVirtualHostByName(name);
	if(vh==NULL){
		ctx->setStatus("vh cann't find");
		return WHM_PARAM_ERROR;
	}
	vhd.delVirtualHost(uv->attribute);
	conf.gvm->removeVirtualHost(vh);
	int ret = WHM_OK;
	ctx->buildVh(vh);
	if(uv->get("destroy")=="1" || uv->get("destroy")=="true"){
		KTempleteVirtualHost *tvh = vh->tvh;
		if(tvh){
			tvh->destroyEvent(ctx);
		}
	}
	return ret;
}
#endif
static int getVhDomain(WhmContext *ctx)
{
	KUrlValue *uv = ctx->getUrlValue();
	string name = uv->get("name");
	KVirtualHost *vh = conf.gvm->refsVirtualHostByName(name);
	if(vh==NULL){
		ctx->setStatus("vh cann't find");
		return WHM_PARAM_ERROR;
	}
	list<KSubVirtualHost *>::iterator it;
	for (it = vh->hosts.begin(); it != vh->hosts.end(); it++) {
		if(!(*it)->allSuccess){
			continue;
		}
		ctx->add("domain",(*it)->host);
	}
	vh->getParsedFileExt(ctx);
	vh->destroy();
	return WHM_OK;
}

static int getVhDetail(WhmContext *ctx)
{
	KUrlValue *uv = ctx->getUrlValue();
	string name = uv->get("name");
	KVirtualHost *vh = conf.gvm->refsVirtualHostByName(name);
	if(vh==NULL){
		ctx->setStatus("vh cann't find");
		return WHM_PARAM_ERROR;
	}
	//ctx->setStatus(WHM_OK);
	stringstream s;
	ctx->add("name",name);
#ifdef ENABLE_BASED_PORT_VH
	{
		list<string>::iterator it2;
		for (it2=vh->binds.begin();it2!=vh->binds.end();it2++) {
			s << (*it2) << "\n";
		}
	}
	ctx->add("bind",s.str().c_str());
	s.str("");
#endif
	{
		list<KSubVirtualHost *>::iterator it;
		for (it = vh->hosts.begin(); it != vh->hosts.end(); it++) {
			s << (*it)->host;
			if (strcmp((*it)->dir, "/") != 0) {
				s << "|" << (*it)->dir;
			}
			s << "\n";
		}
	}
	ctx->add("host",s.str().c_str());
	s.str("");
	ctx->add("doc_root",vh->orig_doc_root);
	ctx->add("inherit",vh->inherit?"1":"0");
#ifdef ENABLE_VH_RUN_AS
	ctx->add("user",vh->user);
#ifdef _WIN32
	ctx->add("password","***");
#else
	ctx->add("group",vh->group);
#endif
#endif

#ifdef ENABLE_VH_LOG_FILE
	ctx->add("log_file",vh->logFile);
	string rotateTime;
	if (vh->logger) {
		vh->logger->getRotateTime(rotateTime);
		ctx->add("log_rotate_time",rotateTime);
		ctx->add("log_rotate_size",vh->logger->rotateSize);
	}
#endif
	ctx->add("browse",vh->browse?"1":"0");
#ifdef ENABLE_USER_ACCESS
	ctx->add("access_file",vh->user_access);
#endif
#ifdef ENABLE_VH_RS_LIMIT
	ctx->add("connect",vh->max_connect);
	ctx->add("speed_limit",vh->speed_limit);
	//ctx->add("flow_limit",vh->flow_limit);
	//ctx->add("flow",vh->flow);
#endif
	vh->destroy();
	return WHM_OK;
}
int WINAPI WhmCoreCall(const char *callName, const char *event, WHM_CONTEXT *context) {
	WhmContext *ctx = (WhmContext *) context->ctx;
	//KWStream *out = ctx->getOutputStream();
	KUrlValue *uv = ctx->getUrlValue();
	std::string errMsg;
	int cmd = parseCallName(callName);
	switch(cmd) {
	case CALL_INFO:
		{		
			INT64 total_mem_size = 0, total_disk_size = 0;
			get_cache_size(total_mem_size, total_disk_size);
			ctx->add("server", PROGRAM_NAME);
			ctx->add("version", VERSION);
			ctx->add("type",getServerType());
			ctx->add("os",getOsType());

			int total_run_time = (int)(time(NULL) - program_start_time);
			ctx->add("total_run",total_run_time);
			ctx->add("connect",total_connect);
#ifdef ENABLE_STAT_STUB
			ctx->add("request",katom_get64((void *)&kgl_total_requests));
			ctx->add("accept",katom_get64((void *)&kgl_total_accepts));
#endif
			ctx->add("cache_count", cache.getCount());
			ctx->add("cache_mem", total_mem_size);
#ifdef ENABLE_DISK_CACHE
			INT64 total_size, free_size;
			ctx->add("cache_disk", total_disk_size);
			if (get_disk_size(total_size, free_size)) {
				ctx->add("disk_total", total_size);
				ctx->add("disk_free", free_size);
			}
#endif
			int vh_count = conf.gvm->getCount();
			ctx->add("vh",vh_count);
			ctx->add("kangle_home",conf.path.c_str());
#ifdef UPDATE_CODE
			ctx->add("update_code", UPDATE_CODE);
#endif
			ctx->add("open_file_limit", open_file_limit);
			ctx->add("addr_cache", get_addr_cache_count());
			ctx->add("disk_cache_shutdown", (int)cache.is_disk_shutdown());
			return WHM_OK;
		}
#if 0
	case CALL_ADD_VH:
		{
			if(!vhd.addVirtualHost(uv->attribute,ctx,errMsg)){
				ctx->setStatus(errMsg.c_str());
				return WHM_CALL_FAILED;
			}
			return WHM_OK;
		}
	case CALL_UPDATE_VH:
		{
			if(!vhd.updateVirtualHost(ctx,uv->attribute,errMsg)){
				ctx->setStatus(errMsg.c_str());
				return WHM_CALL_FAILED;
			}
			return WHM_OK;
		}
#endif
	case CALL_QUERY_DOMAIN:
	{
		const char *domain = uv->getx("domain");
		if (domain == NULL) {
			ctx->setStatus("missing domain");
			return WHM_CALL_FAILED;
		}
		return conf.gvm->find_domain(domain,ctx);
	}
	case CALL_CLEAN_ALL_CACHE:
	{
		dead_all_obj();
		return WHM_OK;
	}
	case CALL_CACHE_PREFETCH:	
	case CALL_CACHE_INFO:
	case CALL_CLEAN_CACHE:
		{
			KCacheInfo ci;
			memset(&ci,0,sizeof(ci));
			const char *url = uv->getx("url");
			if (url==NULL) {
				ctx->setStatus("url is missing");
				return WHM_CALL_FAILED;
			}
			char *buf = strdup(url);
			char *hot = buf;
			int result = 0;
			for (;;) {
				char *p = strstr(hot,", ");
				if (p) {
					*p = '\0';
				}
				char *u = hot;
				if (cmd==CALL_CACHE_PREFETCH) {
					if (cache_prefetch(hot)) {
						result ++;
					}
				} else if (cmd==CALL_CACHE_INFO) {
					switch (*hot) {
					case '3':
						{
							result += get_cache_info(hot+1,true,&ci);
							break;
						}
					case '0':
						u++;
					default:
						result += get_cache_info(u,false,&ci);
						break;
					}
				} else {
					switch (*hot) {
					case '1':
						{
							//�������ִ�Сд
							KReg reg;
							if(reg.setModel(hot+1,0)){
								result += clean_cache(&reg,0);
							}
							break;
						}
							
					case '2':
						{
							//���򣬲����ִ�Сд
							KReg reg;
							if(reg.setModel(hot+1,PCRE_CASELESS)){
								result += clean_cache(&reg,0);
							}
							break;
						}					
					case '3':
						{
							//ƥ��ǰ�沿��
							result += clean_cache(hot+1,true);
							break;

						}						
					case '0':
						//��ȷƥ��
						u++;
					default:
						//��ȷƥ��
						result += clean_cache(u,false);
						break;
					}
				}
				if (p==NULL) {
					break;
				}
				hot = p+2;
			}
			free(buf);
			if (cmd==CALL_CACHE_INFO) {
				ctx->add("mem_size",ci.mem_size);
				ctx->add("disk_size",ci.disk_size);
			}
			ctx->add("count",result);
			return WHM_OK;
		}		
#ifdef ENABLE_VH_FLOW
	case CALL_DUMP_FLOW:
		{
			const char *prefix = uv->getx("prefix");
			bool revers = atoi(uv->get("revers").c_str())==1;
			int prefix_len = 0;
			if (prefix) {
				prefix_len = strlen(prefix);
			}
			conf.gvm->dumpFlow(ctx,revers,prefix,prefix_len,atoi(uv->get("extend").c_str()));
			return WHM_OK;
		}
	case CALL_DUMP_LOAD:
	{
			const char *prefix = uv->getx("prefix");
			bool revers = atoi(uv->get("revers").c_str())==1;
			int prefix_len = 0;
			if (prefix) {
				prefix_len = strlen(prefix);
			}
			conf.gvm->dumpLoad(ctx,revers,prefix,prefix_len);
			return WHM_OK;
	}
	case CALL_GET_LOAD:
		{
			KVirtualHost *vh = ctx->getVh();
			if (vh==NULL) {
				ctx->setStatus("cann't find vh");
				return WHM_CALL_FAILED;
			}
			bool reset = atoi(uv->get("reset").c_str())==1;
			ctx->add("speed",vh->getSpeed(reset));
#ifdef ENABLE_VH_RS_LIMIT
			ctx->add("connect",vh->getConnectCount());
#endif
			return WHM_OK;
		}
	case CALL_GET_CONNECTION:
		{
			//KVirtualHost *vh = ctx->getVh();
			int totalCount = 0;
			string connectString = selectorManager.getConnectionInfo(totalCount,0,uv->getx("vh"),false);
			ctx->add("count",totalCount);
			ctx->add("connection",connectString.c_str(),true);
			return WHM_OK;
		}
#endif
		
	case CALL_CHECK_SSL:
		{
			KVirtualHost *vh = ctx->getVh();
			if (vh==NULL) {
				ctx->setStatus("cann't find vh");
				return WHM_CALL_FAILED;
			}
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
			ctx->add("ssl",vh->ssl_ctx?1:0);
			if (vh->ssl_ctx) {
				char *result = NULL;
				result = ssl_ctx_var_lookup(vh->ssl_ctx,"NOTBEFORE");
				if (result) {
					ctx->add("not_before", result);
					ssl_var_free(result);
				}
				result = ssl_ctx_var_lookup(vh->ssl_ctx, "NOTAFTER");
				if (result) {
					ctx->add("not_after", result);
					ssl_var_free(result);
				}
				result = ssl_ctx_var_lookup(vh->ssl_ctx, "SUBJECT");
				if (result) {
					ctx->add("subject", result);
					ssl_var_free(result);
				}
			}
			return WHM_OK;
#endif
			ctx->setStatus("ssl sni not support");
			return WHM_CALL_FAILED;
		}
	case CALL_RUNTIME_MODEL:
		{
			const char *name = uv->getx("name");
			if (name==NULL) {
				ctx->setStatus("name param is missing");
				return WHM_PARAM_ERROR;
			}
			return KAccess::whmCallRunTimeModel(name,ctx);
		}
	case CALL_SERVER_INFO:
	{
		const char *name = uv->getx("name");
		if (name == NULL) {
			ctx->setStatus("name param is missing");
			return WHM_PARAM_ERROR;
		}
		KMultiAcserver *rd = cdnContainer.refsMultiServer(name);
		std::stringstream s;
		if (rd) {
			rd->getNodeInfo(s);
			ctx->add("node", s.str().c_str());
			rd->release();
		}
		return WHM_OK;
	}
#ifdef ENABLE_LOG_DRILL
	case CALL_LOG_DRILL:
	{
		flush_log_drill();
		return WHM_OK;
	}
#endif
	case CALL_ADD_TABLE:
	case CALL_EMPTY_TABLE:
	case CALL_DEL_TABLE:
	case CALL_LIST_TABLE:	
	case CALL_LIST_CHAIN:
	case CALL_ADD_CHAIN:
	case CALL_EDIT_CHAIN:
	case CALL_DEL_CHAIN:
		{
			//vh=vhname&access=response|request
			std::string err_msg;
			KVirtualHost *vh = ctx->getVh();
			if(vh==NULL &&  uv->getx("vh")){
				ctx->setStatus("cann't find such vh");
				return WHM_CALL_FAILED;
			}
			const char *access = uv->getx("access");
			if(access==NULL){
				ctx->setStatus("access must be set");
				return WHM_PARAM_ERROR;
			}
			int access_type = 0;
			if(strcasecmp(access,"response")==0){
				access_type = RESPONSE;
			}else if(strcasecmp(access,"request")==0) {
				access_type = REQUEST;
			} else {
				ctx->setStatus("access must be response or request");
				return WHM_PARAM_ERROR;
			}
			KAccess *maccess = &kaccess[access_type];
			if(vh){
#ifndef HTTP_PROXY
				if(vh->user_access.size()==0){
					ctx->setStatus("the vh does not support user_access");
					return WHM_CALL_FAILED;
				}
				maccess = &vh->access[access_type];
#endif
			}
			bool result = true;
			bool save_file = false;
			if (cmd==CALL_LIST_TABLE) {
				maccess->listTable(ctx);						
			} else if (cmd==CALL_ADD_TABLE) {
				result = maccess->newTable(uv->get("table_name"),err_msg);
				save_file = true;
			} else if(cmd==CALL_DEL_TABLE) {
				result = maccess->delTable(uv->get("table_name"),err_msg);
				save_file = true;
			} else if(cmd==CALL_EMPTY_TABLE) {
				result = maccess->emptyTable(uv->get("table_name"),err_msg);
				save_file = true;
			} else if(cmd==CALL_LIST_CHAIN) {
				const char *name = uv->getx("name");
				int flag = 0;
				if (atoi(uv->get("detail").c_str())==0) {
					flag |= CHAIN_XML_SHORT;
				}
				if (atoi(uv->get("reset_flow").c_str())==0) {
					flag |= CHAIN_RESET_FLOW;
				}
				result = maccess->listChain(uv->get("table_name"),name,ctx,flag);
			} else if(cmd==CALL_EDIT_CHAIN) {
				const char *id = uv->getx("id");
				const char *name = uv->getx("name");
				if(id==NULL && name==NULL){
					ctx->setStatus("id or name must set one");
				}else{
					if (id) {
						result = maccess->editChain(uv->get("table_name"),atoi(id),uv);
					}else{
						result = maccess->editChain(uv->get("table_name"),name,uv);
					}
				}
				save_file = true;
			} else if(cmd==CALL_ADD_CHAIN) {
				int id = maccess->newChain(uv->get("table_name"),atoi(uv->get("id").c_str()),uv);
				save_file = true;
				ctx->add("id",id);
			} else if(cmd==CALL_DEL_CHAIN) {
				const char *id = uv->getx("id");
				const char *name = uv->getx("name");
				if(id==NULL && name==NULL){
					ctx->setStatus("id or name must set one");
				}else{
					if (id) {
						result = maccess->delChain(uv->get("table_name"),atoi(id));
					}else{
						result = maccess->delChain(uv->get("table_name"),name);
					}
				}
				save_file = true;
			}
			if(!result){
				ctx->setStatus(err_msg.c_str());
				return WHM_CALL_FAILED;
			}
			if(save_file){
				if(vh==NULL){
					KConfigBuilder::saveConfig();
				} else {
#ifndef HTTP_PROXY
					vh->saveAccess();
#endif
				}
			}
			return WHM_OK;
		}
	}
#if 0
	if (cmd == CALL_ADD_VH_INFO) {
		if(!vhd.addInfo(uv->attribute,errMsg)){
			ctx->setStatus(errMsg.c_str());
			return WHM_CALL_FAILED;
		}
		return WHM_OK;
	}
	if (cmd == CALL_DEL_VH_INFO) {
		if (!vhd.delInfo(uv->attribute,errMsg)){
			ctx->setStatus(errMsg.c_str());
			return WHM_CALL_FAILED;
		}
		return WHM_OK;
	}
	if(cmd == CALL_DEL_VH){
		return deleteVh(ctx);
	}
#endif
	if(cmd == CALL_LIST_VH || cmd == CALL_LIST_TVH || cmd == CALL_LIST_GTVH){
		//ctx->setStatus(WHM_OK);
		std::list<std::string> vhs;
		if(cmd==CALL_LIST_VH){
			conf.gvm->getAllVh(
				vhs,
				uv->get("status")=="1",
				uv->get("onlydb")=="1"
				);
		}else if(cmd==CALL_LIST_TVH){
			conf.gvm->getAllTempleteVh(uv->getx("name"),vhs);			
		}else{
			conf.gvm->getAllGroupTemplete(vhs);
		}
		std::list<std::string>::iterator it;
		for(it=vhs.begin();it!=vhs.end();it++){
			ctx->add("name",(*it));
		}
		return WHM_OK;
	}
	if(cmd==CALL_INFO_DOMAIN){
		return getVhDomain(ctx);
	}
	if(cmd==CALL_INFO_VH){
		return getVhDetail(ctx);
	}
	if(cmd==CALL_ADD_ERROR_PAGE 
		|| cmd==CALL_DEL_ERROR_PAGE 
		|| cmd==CALL_ADD_REDIRECT 
		|| cmd==CALL_DEL_REDIRECT
		|| cmd==CALL_ADD_INDEX
		|| cmd==CALL_DEL_INDEX
		|| cmd==CALL_LIST_INDEX
		){
		std::string name;
		KBaseVirtualHost *bvh = ctx->getVh();
		if(bvh==NULL){
			ctx->setStatus("no such vh");
			return WHM_CALL_FAILED;
		}
		bool result=false;
		if(cmd==CALL_ADD_ERROR_PAGE){
			result = bvh->addErrorPage(atoi(uv->get("code").c_str()),uv->get("url"));
		}else if(cmd==CALL_DEL_ERROR_PAGE){
			result = bvh->delErrorPage(atoi(uv->get("code").c_str()));
		}else if(cmd==CALL_ADD_REDIRECT){
			result = bvh->addRedirect(atoi(uv->get("file_ext").c_str())==1,
										uv->get("value"),
										uv->get("target"),
										uv->get("method"),
										atoi(uv->get("exsit").c_str())==1,
										uv->get("params")
										);
		}else if(cmd==CALL_DEL_REDIRECT){
			result = bvh->delRedirect(atoi(uv->get("file_ext").c_str())==1,
										uv->get("value"));
		}else if(cmd==CALL_ADD_INDEX){
			int id = atoi(uv->get("id").c_str());
			if (id==0) {
				id = 80;
			}
			result = bvh->addIndexFile(uv->get("index"),id);
		}else if(cmd==CALL_DEL_INDEX){
			result = bvh->delIndexFile(uv->get("index"));
		} else if(cmd==CALL_LIST_INDEX) {
			bvh->listIndex(ctx);
			result = true;
		}
		if(result){
			return WHM_OK;
		}
		//ctx->setStatus(WHM_CALL_FAILED);
		return WHM_CALL_FAILED;
	}
	if(cmd==CALL_EXPORT_VH){
		std::stringstream s;
		std::string name;
		if(uv->get("name",name)){
			KVirtualHost *vh = conf.gvm->refsVirtualHostByName(name);
			if(vh==NULL){
				ctx->setStatus("vh not found");
				return WHM_CALL_FAILED;
			}
			vh->buildXML(s);
			vh->destroy();
		}else{
			conf.gvm->build(s);
		}
		ctx->add("data",b64encode((const unsigned char *)s.str().c_str()).c_str());
		return WHM_OK;
	}
	if(cmd==CALL_SAVE_VH){
		std::string errMsg;
		if(!conf.gvm->saveConfig(errMsg)){
			ctx->setStatus(errMsg.c_str());
			return WHM_CALL_FAILED;
		}
		return WHM_OK;
	}
	if(cmd==CALL_EXPORT_CONFIG){
		KConfigBuilder builder;
		std::stringstream s;
		builder.build(s);
		ctx->add("data",b64encode((const unsigned char *)s.str().c_str()).c_str());
		return WHM_OK;
	}
	if(cmd==CALL_SAVE_CONFIG){
		if(!KConfigBuilder::saveConfig()){
			return WHM_CALL_FAILED;
		}
		return WHM_OK;
	}
	if(cmd==CALL_ADD_SERVER){
		std::string err_msg;
		bool result = conf.gam->newSingleAcserver(
			atoi(uv->get("over_flag").c_str())==1,
			uv->attribute,
			err_msg);
		if(!result){
			ctx->setStatus(err_msg.c_str());
			return WHM_CALL_FAILED;
		}
		return WHM_OK;
	}
	if(cmd==CALL_DEL_SERVER){
		std::string err_msg;
		bool result = conf.gam->delAcserver(uv->get("name"),err_msg);
		if(!result){
			ctx->setStatus(err_msg.c_str());
			return WHM_CALL_FAILED;
		}
		return WHM_OK;
	}
	if(cmd==CALL_KILL_PROCESS){	
		if(!killProcess(ctx->getVh())){
			//ctx->setStatus(WHM_CALL_FAILED);
			return WHM_CALL_FAILED;	
		}
		return WHM_OK;
		//conf.gam->killProcess(uv->get("user"));
	}
	if (cmd==CALL_RELOAD) {
		configReload = true;
		return WHM_OK;
	}
	if(cmd==CALL_RELOAD_VH){
		std::string name;
		bool initEvent = false;
		if (uv->get("init")=="1"||uv->get("init")=="true") {
			initEvent = true;
		}
		if (uv->get("name",name)) {
			if (!vhd.flushVirtualHost(name.c_str(),initEvent,ctx)) {
				return WHM_CALL_FAILED;
			}
		} else {
			const char *names = uv->getx("names");
			if (names && *names) {
				//�ж��
				char *buf = strdup(names);
				char *hot = buf;
				for (;;) {
					char *p = strchr(hot,',');
					if (p) {
						*p = '\0';
					}
					if (*hot) {
						vhd.flushVirtualHost(hot,initEvent,ctx);
					}
					if (p==NULL) {
						break;
					}
					hot = p+1;
				}
				free(buf);
			} else {
				configReload = true;
			}
		}
		return WHM_OK;
	}
	if(cmd==CALL_CHANGE_ADMIN_PASSWORD){
		std::string errMsg;
		conf.admin_lock.Lock();
		if(!changeAdminPassword(uv,errMsg)){
			conf.admin_lock.Unlock();
			ctx->setStatus(errMsg.c_str());
			return WHM_CALL_FAILED;
		}
		conf.admin_lock.Unlock();
		KConfigBuilder::saveConfig();
		return WHM_OK;
	}
	if(cmd==CALL_REBOOT){
		console_call_reboot();
		return WHM_OK;
	}
	if(cmd==CALL_WRITE_FILE){
		std::string file = uv->get("file");
		if(file.size()<=0){
			return WHM_CALL_FAILED;
		}
		std::string content = uv->get("content");
		std::string urlencode = uv->get("urlencode");
		if(isAbsolutePath(file.c_str())){
			#ifdef _WIN32
			if(file[0]=='/'){
				file = conf.diskName + file;
			}
			#endif	
		}else{
			file = conf.path + file;
		}
		int len = content.size();
		char *buf = NULL;
		if(urlencode.size()>0 && urlencode=="1"){
			buf = strdup(content.c_str());
		} else {
			buf = b64decode((const unsigned char *)content.c_str(),&len);
			if(buf==NULL || len<=0){
				ctx->setStatus("cann't decode content");
				if(buf){
					xfree(buf);
				}
				return WHM_CALL_FAILED;
			}			
		}
		FILE *fp = fopen(file.c_str(),"wb");
		if(fp==NULL){
			ctx->setStatus("access denied");
			if(buf){
				xfree(buf);
			}
			return WHM_CALL_FAILED;
		}
		fwrite(buf,1,len,fp);
		fclose(fp);
		if (buf) {
			xfree(buf);
		}
		return WHM_OK;	
	}
	if(cmd==CALL_CHECK_VH_DB){
		if(vhd.check()){
			ctx->add("status","1");
		}else{
			ctx->add("status","0");
		}
		return WHM_OK;
	}
	return WHM_CALL_NOT_FOUND;
}
