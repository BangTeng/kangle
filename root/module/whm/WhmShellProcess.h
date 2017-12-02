#ifndef WHMSHELLPROCESS_H
#define WHMSHELLPROCESS_H
#include <vector>
#include "WhmShellContext.h"
#include "KFile.h"
class WhmShellCommand
{
public:
	WhmShellCommand()
	{
		next = NULL;
		cmd = NULL;
	}
	~WhmShellCommand()
	{
		if (cmd) {
			xfree(cmd);
		}
	}
	char **makeArgs(WhmShellContext *sc)
	{
		int args_count = args.size() + 1;
		char **arg = new char *[args_count];
		int i=0;
		for (unsigned j=0; j < args.size(); j++) {
			char *a = sc->parseString(args[j]);
			if(a==NULL){
				continue;
			}
			if(*a=='\0'){
				free(a);
				continue;
			}
			arg[i] = a;
			i++;
		}
		arg[i] = NULL;
		return arg;
	}
	KCmdEnv *makeEnv(WhmShellContext *sc)
	{
		return NULL;
	}
	void setCmd(const char *cmd)
	{
		orig_cmd = cmd;
		this->cmd = strdup(cmd);
		explode_cmd(this->cmd,args);
	}
	std::string orig_cmd;
	char *cmd;
	std::vector<char *> args;
	WhmShellCommand *next;
};
/**
* whm shellһ���������
* ��Ӧһ����̰�ǰ��˳��ͨ���ܵ�����.
*/
class WhmShellProcess
{
public:
	WhmShellProcess()
	{
		stdin_file = NULL;
		stdout_file = NULL;
		stderr_file = NULL;
		next = NULL;
		last = NULL;
		curdir = NULL;
		command = NULL;
		daemon = false;
		runAsUser = true;
	}
	~WhmShellProcess()
	{
		if (stdin_file) {
			xfree(stdin_file);
		}
		if (stdout_file) {
			xfree(stdout_file);
		}
		if (stderr_file) {
			xfree(stderr_file);
		}
		if (curdir) {
			free(curdir);
		}
		while (command) {
			WhmShellCommand *n = command->next;
			delete command;
			command = n;
		}
	}
	WhmShellProcess *clone()
	{
		WhmShellProcess *process = new WhmShellProcess;
		process->daemon = daemon;
		if(stdin_file){
			process->stdin_file = strdup(stdin_file);
		}
		if(stdout_file){
			process->stdout_file = strdup(stdout_file);
		}
		if(stderr_file){
			process->stderr_file = strdup(stderr_file);
		}
		process->runAsUser = runAsUser;
		WhmShellCommand *cmd = command;
		while (cmd) {
			process->addCommand(cmd->orig_cmd.c_str());
			cmd = cmd->next;
		}
		return process;
	}
	PIPE_T get_stdin(WhmShellContext *sc)
	{
		return get_file(sc,stdin_file,true);
	}
	PIPE_T get_stdout(WhmShellContext *sc)
	{
		return get_file(sc,stdout_file,false);
	}
	PIPE_T get_stderr(WhmShellContext *sc)
	{
		return get_file(sc,stderr_file,false);
	}
	//�Ƿ��ں�̨���С�������򲻵ȴ�������̵��˳���
	bool daemon;
	//��׼��������ļ���
	char *stdin_file;
	char *stdout_file;
	char *stderr_file;
	char *curdir;
	bool run(WhmShellContext *context);
	void addCommand(const char *cmd);
	bool runAsUser;
	WhmShellProcess *next;
private:
	PIPE_T get_file(WhmShellContext *sc,const char *file,bool isRead)
	{
		if (file==NULL) {
			return INVALIDE_PIPE;
		}
		char *f = sc->parseString(file);
		if (f==NULL) {
			return INVALIDE_PIPE;
		}
		file = f;
		fileModel model;
		if (isRead) {
			model = fileRead;
		} else {
			if (*file=='+') {
				file++;
				model = fileAppend;
			} else {
				model = fileWrite;
			}
		}
		KFile fp;
		fp.open(file,model);
		free(f);
		return fp.stealHandle();
	}
	WhmShellCommand *command;
	WhmShellCommand *last;
};
#endif
