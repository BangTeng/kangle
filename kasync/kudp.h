#ifndef KUDP_H_9fff9
#define KUDP_H_9fff9
#include "ksocket.h"
#include "kselectable.h"
KBEGIN_DECLS
typedef struct kudp_server_s kudp_server;

typedef void(*kudp_accept_callback)(kudp_server *us, char *buffer, int len, sockaddr_i *addr);

struct kudp_server_s {
	kselectable st;
};
typedef struct {
	kselectable st;
} kudp_client;

void kudp_server_free(kudp_server *us);
kudp_server *kudp_server_listen(const char *ip, uint16_t port);
kudp_client *kudp_new_client(const sockaddr_i *src);
void kudp_free_client(kudp_client *uc);
bool kudp_send(kudp_client *uc,const sockaddr_i *dst,const char *package, int package_len);
void kudp_server_accept(kudp_server *us, kudp_accept_callback accept_callback, int max_length);
kev_result kudp_recv_from(kudp_client *uc, void *arg,result_callback result, buffer_callback buffer, buffer_callback addr);
KEND_DECLS
#endif

