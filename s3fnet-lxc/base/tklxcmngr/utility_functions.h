#include "socket_hooks/hook_defs.h"
int send_to_timekeeper(char * cmd);
void add_lxc_to_socket_monitor(int pid, char * lxcName);
void stop_socket_hook();
void start_socket_hook();
void load_lxc_latest_info(char * lxcName);
int send_to_socket_hook_monitor(char * cmd);
int gettid();
int is_root();
int isModuleLoaded();
int fixDilation(double dilation);
int getpidfromname(char *lxcname);
