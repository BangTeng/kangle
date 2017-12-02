#ifndef MAIN_H_3523213E1QWE3R1W3ER13W2ER1XC1V3D1S3F
#define MAIN_H_3523213E1QWE3R1W3ER13W2ER1XC1V3D1S3F
void my_exit(int code);
void shutdown_signal(int sig);
void shutdown();
void check_graceful_shutdown();
void reloadVirtualHostConfig();
int stop(int service);
void sigcatch(int sig);
void service_from_signal();
void save_pid();
int create_file_path(char **argv);
void restore_pid();
int parse_args(int argc, char ** argv);
void init_daemon();
void init_program();
int start(int service);
int main(int argc, char **argv);
int forward_signal(const char *protocol);
int get_service(const char * service);
const char * get_service_name(int service);
int get_service_id(const char * service);
void set_user(const char *user);
void console_call_reboot();
void clean_process(int pid);
void init_safe_process();
extern int m_pid;
extern int m_ppid;
extern volatile bool configReload;
extern bool dump_memory_object;
extern volatile bool cur_config_ext;
extern volatile bool cur_config_vh_db;
extern int worker_index;
#ifdef ENABLE_VH_FLOW
extern volatile bool flushFlowFlag;
#endif
#endif
