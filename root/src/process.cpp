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
#include "KString.h"
#include "utils.h"
#include "KPipeStream.h"
#include "log.h"
#include "extern.h"
volatile int quit_program_flag = PROGRAM_NO_QUIT;
#ifndef _WIN32
int my_uid = -1;
#endif
static bool open_std_file(const char *filename,KFile *file)
{
	if (*filename == '+') {
		if (!file->open(filename+1,fileAppend)) {
			return false;
		}
	} else {
		if (!file->open(filename,fileWrite)) {
			return false;
		}
	}
	return true;
}
bool open_process_std(kgl_process_std *std,KFile *file)
{
	if (std->hstdin==INVALIDE_PIPE && std->stdin_file) {
		if (!file[0].open(std->stdin_file,fileRead)) {
			return false;
		}
		std->hstdin = file[0].getHandle();
	}
	if (std->hstdout==INVALIDE_PIPE && std->stdout_file) {
		if (!open_std_file(std->stdout_file,&file[1])) {
			return false;
		}
		std->hstdout = file[1].getHandle();
	}
	if (std->hstderr == INVALIDE_PIPE && std->stderr_file) {
		if (kflike(file[1].getHandle()) && filecmp(std->stderr_file,std->stdout_file)==0) {
			std->hstderr = std->hstdout;
		} else {
			if (!open_std_file(std->stderr_file,&file[2])) {
				return false;
			}
			std->hstderr = file[2].getHandle();
		}
	}
	return true;
}

void closeAllFile(int start_fd) {
#ifndef _WIN32
	int max_fd = sysconf(_SC_OPEN_MAX);
	if (max_fd == -1) {
		max_fd = 2048;
	}
	for (int i = start_fd; i < max_fd; i++) {
		::close(i);
	}
#endif
}
void append_cmd_arg(KStringBuf &s , const char *arg)
{
	const char *hot = arg;
	char lastChar='\0';
	while(*hot){
		if(*hot == '"'){
			s << "\\";
		}
		lastChar = *hot;
		s << *hot;
		hot++;
	}
	if (lastChar=='\\') {
		s << "\\";
	}
}

bool createProcess(Token_t token, char * args[],KCmdEnv *envs,char *curdir,PIPE_T in,PIPE_T out,PIPE_T err,pid_t &pid)
{
	if (quit_program_flag==PROGRAM_QUIT_IMMEDIATE) {
		return false;
	}
	if (args == NULL || args[0] == NULL) {
		return false;
	}
	KStringBuf arg(512);
	for(int i=0;;i++) {
		if(args[i]==NULL) {
			break;
		}
		if(i>0) {
			arg << " ";
		}
		arg << "\"";
		append_cmd_arg(arg,args[i]);
		arg << "\"";
	}
	klog(KLOG_NOTICE,"now create process [%s]\n",arg.getString());
	
	pid = fork();
	if (pid == -1) {
		klog(KLOG_ERR, "cann't fork errno=%d\n", errno);
		return false;
	}	
	if (pid == 0) {
		signal(SIGTERM, SIG_DFL);
		if (token && my_uid == 0) {
			setgid(token[1]);
			setuid(token[0]);
		}
#if 0
		if (conf.program.size() == 0) {
			debug("api_child_start failed\n");
			delete st;
			exit(0);
		}
#endif
		//	printf("socket = %d\n",st->fd[0]);
		//st->closeAllOtherFile();
		//printf("socket = %d\n",st->fd[0]);
		if (in>0) {
			close(0);
			dup2(in,0);
		}
		if (out>=0 && out!=1) {
			close(1);
			dup2(out,1);
		}
		if (err>=0 && err!=2) {
			close(2);
			dup2(err,2);
		}
		closeAllFile(3);
		if (curdir==NULL) {
			curdir = strdup(args[0]);
			char *p = strrchr(curdir,'/');
			if (p) {
				*p = '\0';
			}
			chdir(curdir);
			free(curdir);
		} else {		
			chdir(curdir);
		}
		execve(args[0], args, (envs ? envs->getEnv() : NULL));
		//execv(args[0], args);
		fprintf(stderr, "run cmd[%s] error=%d %s\n", args[0], errno, strerror(
			errno));
		debug("child end\n");
		exit(127);	
	}
	
	return true;
}
bool createProcess(KPipeStream *st, Token_t token, char * args[],KCmdEnv *envs, int rdstd) {
	if (quit_program_flag==PROGRAM_QUIT_IMMEDIATE) {
		return false;
	}
	bool pipeCreated = false;
	if (
#ifndef _WIN32
		rdstd==RDSTD_NAME_PIPE || 
#endif
		rdstd==RDSTD_ALL){
			pipeCreated = true;  
			if(!st->create()) {
				return false;
			}
	}
	if (args == NULL || args[0] == NULL) {
		return false;
	}
	KStringBuf arg(512);
	for(int i=0;;i++) {
		if(args[i]==NULL) {
			break;
		}
		if(i>0) {
			arg << " ";
		}
		arg << "\"";
		append_cmd_arg(arg,args[i]);
		arg << "\"";
	}
	klog(KLOG_NOTICE,"now create process [%s]\n",arg.getString());
	
	int c_pid = fork();
	if (c_pid == -1) {
		klog(KLOG_ERR, "cann't fork errno=%d\n", errno);
		return false;
	}
	if(pipeCreated){
		st->setPipe(c_pid);
	}else{
		st->process.bind(c_pid);
	}
	if (c_pid == 0) {
		signal(SIGTERM, SIG_DFL);
		if (token && my_uid == 0) {
			setgid(token[1]);
			setuid(token[0]);
		}
#if 0
		if (conf.program.size() == 0) {
			debug("api_child_start failed\n");
			delete st;
			exit(0);
		}
#endif
		//	printf("socket = %d\n",st->fd[0]);
		st->closeAllOtherFile();
		//printf("socket = %d\n",st->fd[0]);
		if (rdstd == RDSTD_ALL) {
			close(0);
			close(1);
			dup2(st->fd[0], 0);
			dup2(st->fd[1], 1);
			close(st->fd[0]);
			close(st->fd[1]);
		} else if (rdstd == RDSTD_NAME_PIPE) {
			close(4);
			close(5);
			dup2(st->fd[0], 4);
			dup2(st->fd[1], 5);
			close(st->fd[0]);
			close(st->fd[1]);
		} else if (rdstd == RDSTD_INPUT) {
			close(0);
			dup2(st->fd[0], 0);
			close(st->fd[0]);
			//close(1);
			//close(2);
		}
		char *curdir = strdup(args[0]);
		char *p = strrchr(curdir,'/');
		if(p){
			*p = '\0';
		}
		chdir(curdir);
		free(curdir);
		execve(args[0], args, (envs ? envs->getEnv() : NULL));
		//execv(args[0], args);
		fprintf(stderr, "run cmd[%s] error=%d %s\n", args[0], errno, strerror(
			errno));
		debug("child end\n");
		exit(0);
	}
	
	return true;

}
KPipeStream * createProcess(Token_t token, char * args[],
							KCmdEnv *envs, int rdstd) {
								if (args == NULL && args[0] == NULL) {
									return NULL;
								}
								KPipeStream *st = new KPipeStream();
								if (!createProcess(st, token, args, envs, rdstd)) {
									delete st;
									return NULL;
								}
								return st;
}
