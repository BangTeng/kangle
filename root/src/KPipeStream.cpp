/*
 * KPipeStream.cpp
 *
 *  Created on: 2010-6-11
 *      Author: keengo
 */
#ifndef _WIN32
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#endif
#include <fcntl.h>
#include <stdio.h>
#include "log.h"
#include "KPipeStream.h"
#include "forwin32.h"
#include "utils.h"
#include "malloc_debug.h"
using namespace std;
KPipeStream::KPipeStream() {
	fd[0] = fd[1] = INVALIDE_PIPE;
	fd2[0] = fd2[1] = INVALIDE_PIPE;
#ifndef _WIN32
	tmo = 0;
#endif
	errorCount = 0;
}
void KPipeStream::waitClose()
{
#ifdef _WIN32
	
#else
	char buf[32];
	for(;;){
		int len = read(buf,sizeof(buf)-1);
		if(len<=0){
			return;
		}
		buf[len] = '\0';
		printf("%s",buf);
	}
#endif
}
void KPipeStream::closeAllOtherFile() {
#ifndef _WIN32
	int max_fd = sysconf(_SC_OPEN_MAX);
	if (max_fd == -1) {
		//		debug("cann't get max_fd=%d\n", max_fd);
		max_fd = 2048;
	}
	int start_fd = 3;
	if (m_debug > 0) {
		/*
		 * 如果是调试，我们不关stdin,stdout,stderr,方便调试。
		 */
		start_fd = 3;
	}
	//	debug("max_fd=%d\n",max_fd);
	for (int i = start_fd; i < max_fd; i++) {
		if (i != fd[0] && i != fd[1]) {
			::close(i);
		}
	}
#endif
}
void KPipeStream::close() {
	if (fd[0] != INVALIDE_PIPE) {
		ClosePipe(fd[0]);
		fd[0] = INVALIDE_PIPE;
	}
	if (fd[1] != INVALIDE_PIPE) {
		ClosePipe(fd[1]);
		fd[1] = INVALIDE_PIPE;
	}
	if (fd2[0] != INVALIDE_PIPE) {
		ClosePipe(fd2[0]);
		fd2[0] = INVALIDE_PIPE;
	}
	if (fd2[1] != INVALIDE_PIPE) {
		ClosePipe(fd2[1]);
		fd2[1] = INVALIDE_PIPE;
	}
}
KPipeStream::~KPipeStream() {
	close();
}

void KPipeStream::setPipe(pid_t child_pid) {
	if (child_pid == 0) {
		//it is main
		ClosePipe(fd[1]);
		ClosePipe(fd2[0]);
		fd[1] = fd2[1];
	} else {
		ClosePipe(fd[0]);
		ClosePipe(fd2[1]);
		fd[0] = fd2[0];
	}
	fd2[0] = INVALIDE_PIPE;
	fd2[1] = INVALIDE_PIPE;
	process.bind(child_pid);
//	pid = child_pid;
}
bool KPipeStream::create() {
	if (!create(fd)) {
		return false;
	}
	return create(fd2);
}
void KPipeStream::setTimeOut(int tmo) {
	
	this->tmo = tmo;

}
bool KPipeStream::create(PIPE_T *fd) {
	
	return pipe(fd) == 0;

}
int KPipeStream::read(char *buf, int len) {

	if (!waitForRW(fd[READ_PIPE], false, tmo)) {
		killChild();
	}
	return ::read(fd[READ_PIPE], buf, len);

}
int KPipeStream::write(const char *buf, int len) {
	
	if (!waitForRW(fd[WRITE_PIPE], true, tmo)) {
		killChild();
	}
	return ::write(fd[WRITE_PIPE], buf, len);

}
void KPipeStream::killChild() {
	process.kill();
}
bool KPipeStream::writeString(const char *str) {
	int len = 0;
	if (str) {
		len = strlen(str);
	}
	if (!write_all((char *) &len, sizeof(len))) {
		return false;
	}
	if (str) {
		return write_all(str, len) == STREAM_WRITE_SUCCESS;
	}
	return true;
}
char *KPipeStream::readString(bool &result) {
	int len;
	if (!read_all((char *) &len, sizeof(len))) {
		result = false;
		return NULL;
	}
	if (len <= 0) {
		result = true;
		return NULL;
	}
	char *str = (char *) xmalloc(len+1);
	if (str == NULL) {
		result = false;
		return NULL;
	}
	str[len] = '\0';
	result = read_all(str, len);
	if (!result) {
		xfree(str);
		return NULL;
	}
	return str;
}
bool KPipeStream::writeInt(int value) {
	return write_all((char *) &value, sizeof(value)) == STREAM_WRITE_SUCCESS;
}
int KPipeStream::readInt(bool &result) {
	int value = 0;
	result = read_all((char *) &value, sizeof(value));
	return value;
}
bool KPipeStream::create_name(const char *read_pipe, const char *write_pipe) {

	return true;
}
bool KPipeStream::connect_name(const char *read_pipe, const char *write_pipe) {

	return (fd[WRITE_PIPE] != INVALIDE_PIPE);
}
