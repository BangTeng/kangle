#ifndef WHMSHELL_H
#define WHMSHELL_H
/**
* whmshell ��
* ͨ��whm������ɼ򵥵���������Ĺ��ܡ����httpЭ���ص㣬��֧��ͬ�����첽���ֵ��÷�ʽ��
* �첽���ò��õ�������ɼ��ɷ��أ�֮��ͨ�����ò�ѯ�����ѯ���ȼ�������Ϣ
* �﷨:
* <extend name='����' type='whmshell' model='<async|sync>'/>
* <commands [condition='condition'] [stdin='file'] [stdout='file'] [stderr='file'] [daemon='on|off'] runas='<system|user>'>
* <command>command</command>
* ...
* </commands>
* </extend>
*/

#include "WhmExtend.h"
#include "WhmShellContext.h"
#include "WhmShellProcess.h"

class WhmShell: public WhmExtend {
public:
	WhmShell();
	~WhmShell();
	bool startElement(KXmlContext *context, std::map<std::string,std::string> &attribute);
	bool startCharacter(KXmlContext *context, char *character, int len);
	bool endElement(KXmlContext *context);
	void asyncRun(WhmShellContext *sc);
	void run(WhmShellContext *sc);
	const char *getType()
	{
		return "shell";
	}
	void flush();
	WhmShellContext *refsContext(std::string session);
	bool removeContext(WhmShellContext *sc);
	void endContext(WhmShellContext *sc);
	void include(WhmShell *shell);
	int result(WhmShellContext *sc,WhmContext *context);
	bool async;
	bool merge;
protected:
	int call(const char *callName,const char *eventType,WhmContext *context);
private:
	void initContext(WhmShellContext *sc,WhmContext *context);
	void readStdin(WhmShellContext *sc,WhmContext *context);
	void addContext(WhmShellContext *sc);
	KMutex lock;
	unsigned index;
	std::map<std::string,WhmShellContext *> context;
	//�Ѿ����н������ȴ�flush�ͽ�����shell context
	WhmShellContext *head;
	WhmShellContext *end;

	WhmShellContext *merge_context;
	volatile bool merge_context_running;
	void addProcess(WhmShellProcess *p);
	WhmShellProcess *process;
	WhmShellProcess *last;
	WhmShellProcess *curProcess;
};
#endif
