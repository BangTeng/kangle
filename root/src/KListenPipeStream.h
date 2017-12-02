#ifndef KLISTENPIPESTREAM_H
#define KLISTENPIPESTREAM_H
#include "KPipeStream.h"
#include "global.h"
#include "KListNode.h"
#include "KPoolableSocketContainer.h"
class KListenPipeStream : public KPipeStream
{
public:
	KListenPipeStream()
	{
		port = 0;
		server = NULL;
	}
	virtual ~KListenPipeStream()
	{
		closeServer();
	}
	void unlink_unix()
	{
#ifdef KSOCKET_UNIX
		if(unix_path.size()>0){
			if (unlink(unix_path.c_str())!=0) {
				int err = errno;
				klog(KLOG_ERR,"cann't unlink unix socket [%s] error =%d %s\n",unix_path.c_str(),err,strerror(err));
			} else {
				unix_path.clear();
			}
		}
#endif
	}
	/*
	KClientSocket *connect()
	{
		if(port == 0){
			return NULL;
		}
		KClientSocket *client = new KClientSocket;
		if(client->connect("127.0.0.1",port,10)){
			client->set_time(60);
			return client;
		}
		delete client;
		return NULL;
	}
	*/
	void closeServer()
	{
		if (server) {
			assert(fd[0]);
			//避免两次关闭
			//fd[0]来自server,参见listen
			fd[0] = INVALIDE_PIPE;
			delete server;
			server = NULL;
		}
	}
#ifdef KSOCKET_UNIX	
	bool listen(const char *path)
	{
		this->unix_path = path;
		assert(server == NULL);
		KUnixServerSocket *userver = new KUnixServerSocket;
		if(!userver->open(path)){
			delete userver;
			unlink(path);
			return false;
		}
		assert(fd[0]==INVALIDE_PIPE);
		server = userver;
#ifdef _WIN32
		fd[0] = (HANDLE)server->get_socket();
#else
		fd[0] = server->get_socket();
#endif
		return true;
	}
#endif
	bool listen(int port=0,const char *host="127.0.0.1")
	{
		assert(server == NULL);
		server = new KServerSocket;
		if (!server->open4(port,host)) {
			delete server;
			server = NULL;
			return false;
		}
		assert(fd[0]==INVALIDE_PIPE);
#ifdef _WIN32
		fd[0] = (HANDLE)server->get_socket();
#else
		fd[0] = server->get_socket();
#endif
		return true;
	}
	int getPort()
	{
		if (server) {
			port = server->get_self_port();
		}
		return port;
	}
	void setPort(int port)
	{
		this->port = port;
	}
	
	friend class KCmdPoolableRedirect;
	std::string unix_path;
private:
	int port;
	KServerSocket *server;
};
#endif
