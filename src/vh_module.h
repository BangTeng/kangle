#ifndef VH_MODULE_H
#define VH_MODULE_H
#ifdef __cplusplus
extern "C" {
#endif
	struct vh_data
	{
		void *ctx;
		void (* set)(void *ctx,const char *name,const char *value);
		/*
		该函数不会返回NULL
		*/
		const char * (* get)(void *ctx,const char *name);
		/*
		该函数会返回NULL
		*/
		const char * (* getx)(void *ctx,const char *name);
	};
	struct vh_module
	{
		int cbsize;
		void *ctx;
		int vhi_version;
		const char *(* getConfigValue)(void *ctx,const char *name);
		//以下是返回
		//解析配置文件
		int (* parseConfig)(vh_data *data);
		//建立连接
		void *(* createConnection)();
		//查询操作,data作为输出参数
		int (*query)(void *stmt,vh_data *data);
		void (*freeStmt)(void *stmt);
		void (*freeConnection)(void *cn);
		//读取操作，返回stmt
		void *(* loadVirtualHost)(void *cn);
		void *(* flushVirtualHost)(void *cn,const char *name);
		void *(* loadInfo)(void *cn,const char *name);
		//更新操作，成功返回1,错误返回0
		int (* addVirtualHost)(void *cn,vh_data *data);
		int (* updateVirtualHost)(void *cn,vh_data *data);
		int (* delVirtualHost)(void *cn,vh_data *data);
		int (* delInfo)(void *cn,vh_data *data);
		int (* addInfo)(void *cn,vh_data *data);
		int (* delAllInfo)(void *cn,vh_data *data);
		//流量操作
		void *(* getFlow)(void *cn,const char *name);
		int (* setFlow)(void *cn,vh_data *data);
	};
	int initVirtualHostModule(vh_module *ctx);
	typedef int (*initVirtualHostModulef)(vh_module *ctx);
#ifdef __cplusplus
}
#endif
#endif
