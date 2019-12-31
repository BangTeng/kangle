#ifndef VH_MODULE_H
#define VH_MODULE_H
#ifdef __cplusplus
extern "C" {
#endif
	typedef void * kgl_vh_connection;
	typedef void * kgl_vh_stmt;
	struct vh_data
	{
		void *ctx;
		void (* set)(void *ctx,const char *name,const char *value);
		/*
		�ú������᷵��NULL
		*/
		const char * (* get)(void *ctx,const char *name);
		/*
		�ú����᷵��NULL
		*/
		const char * (* getx)(void *ctx,const char *name);
	};
	struct vh_module
	{
		int cbsize;
		void *ctx;
		int vhi_version;
		const char *(* getConfigValue)(void *ctx,const char *name);
		//�����Ƿ���
		//���������ļ�
		int (* parseConfig)(vh_data *data);
		//��������
		kgl_vh_connection (* createConnection)();
		//��ѯ����,data��Ϊ�������
		int (*query)(kgl_vh_stmt stmt,vh_data *data);
		void (*freeStmt)(kgl_vh_stmt stmt);
		void (*freeConnection)(kgl_vh_connection cn);
		//��ȡ����������stmt
		kgl_vh_stmt (* loadVirtualHost)(kgl_vh_connection cn);
		kgl_vh_stmt (* flushVirtualHost)(kgl_vh_connection cn,const char *name);
		kgl_vh_stmt (* loadInfo)(kgl_vh_connection cn,const char *name);
		//���²������ɹ�����1,���󷵻�0
		int (* addVirtualHost)(kgl_vh_connection cn,vh_data *data);
		int (* updateVirtualHost)(kgl_vh_connection cn,vh_data *data);
		int (* delVirtualHost)(kgl_vh_connection cn,vh_data *data);
		int (* delInfo)(kgl_vh_connection cn,vh_data *data);
		int (* addInfo)(kgl_vh_connection cn,vh_data *data);
		int (* delAllInfo)(kgl_vh_connection cn,vh_data *data);
		//��������
		void *(*getFlow)(kgl_vh_connection cn,const char *name);
		int (* setFlow)(kgl_vh_connection cn,vh_data *data);
		//�ڰ�����
		kgl_vh_stmt(*loadBlackList)(kgl_vh_connection cn);
	};
	int initVirtualHostModule(vh_module *ctx);
	typedef int (*initVirtualHostModulef)(vh_module *ctx);
#ifdef __cplusplus
}
#endif
#endif
