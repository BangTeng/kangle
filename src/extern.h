#if	!defined(_EXTERN_H_INCLUDED_)
#define _EXTERN_H_INCLUDED_
#define STATE_UNKNOW   0
#define STATE_IDLE     1
#define STATE_SEND     2
#define STATE_RECV     3
#define STATE_QUEUE    4
extern volatile int quit_program_flag;
extern volatile bool autoupdate_thread_started;
extern int my_uid;
extern int api_child_key;
extern int serial;
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
extern bool dump_memory_object;
extern volatile bool cur_config_ext;
extern volatile bool cur_config_vh_db;
extern int worker_index;
extern unsigned total_connect;
#ifdef ENABLE_VH_FLOW
extern volatile bool flushFlowFlag;
#endif
#ifdef _WIN32
void WINAPI serviceMain(DWORD argc, LPTSTR * argv);
bool InstallService(const char * szServiceName, bool install = true, bool start = true);
bool UninstallService(const char * szServiceName, bool uninstall = true);
void LogEvent(LPCTSTR pFormat, ...);
void Start();
void Stop();
#endif
#endif	/* !_EXTERN_H_INCLUDED_ */
