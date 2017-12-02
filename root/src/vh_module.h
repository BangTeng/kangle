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
		void *(* createConnection)();
		//��ѯ����,data��Ϊ�������
		int (*query)(void *stmt,vh_data *data);
		void (*freeStmt)(void *stmt);
		void (*freeConnection)(void *cn);
		//��ȡ����������stmt
		void *(* loadVirtualHost)(void *cn);
		void *(* flushVirtualHost)(void *cn,const char *name);
		void *(* loadInfo)(void *cn,const char *name);
		//���²������ɹ�����1,���󷵻�0
		int (* addVirtualHost)(void *cn,vh_data *data);
		int (* updateVirtualHost)(void *cn,vh_data *data);
		int (* delVirtualHost)(void *cn,vh_data *data);
		int (* delInfo)(void *cn,vh_data *data);
		int (* addInfo)(void *cn,vh_data *data);
		int (* delAllInfo)(void *cn,vh_data *data);
		//��������
		void *(* getFlow)(void *cn,const char *name);
		int (* setFlow)(void *cn,vh_data *data);
	};
	int initVirtualHostModule(vh_module *ctx);
	typedef int (*initVirtualHostModulef)(vh_module *ctx);
#ifdef __cplusplus
}
#endif
#endif
