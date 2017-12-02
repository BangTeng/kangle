/*
 * KPipeStream.h
 *
 *  Created on: 2010-6-11
 *      Author: keengo
 */

#ifndef KPIPESTREAM_H_
#define KPIPESTREAM_H_

#include "forwin32.h"
#include "KProcess.h"
#include "ksapi.h"
#define READ_PIPE		0
#define WRITE_PIPE		1
/*
 * ��pipe���ɵĹܵ���
 */
class KPipeStream: public KStream {
public:
	KPipeStream();
	virtual ~KPipeStream();
	void close();
	int read(char *buf, int len);
	int write(const char *buf, int len);
	bool create();
	bool create_name(const char *read_pipe,const char *write_pipe);
	bool connect_name(const char *read_pipe,const char *write_pipe);
	void setPipe(pid_t child_pid);
	void waitClose();
	PIPE_T fd[2];
	PIPE_T fd2[2];
	bool writeString(const char *str);
	bool writeInt(int value);
	int readInt(bool &result);
	char *readString(bool &result);
	static bool create(PIPE_T *fd);
	void closeAllOtherFile();
	void killChild();
	void setTimeOut(int tmo);	
	/*
		�������
	*/
	int errorCount;
#ifndef _WIN32
	int tmo;
#endif
	KProcess process;

};

#endif /* KPIPESTREAM_H_ */
