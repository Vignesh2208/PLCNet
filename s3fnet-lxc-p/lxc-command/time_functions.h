
int is_root();
int clone_time(unsigned long flags, double dilation, int should_freeze);
int synchronize_pids(int count, ...);
int dilate(int pid, double dilation);
int freeze(int pid);
int unfreeze(int pid);
int freeze_all(int pid);
int unfreeze_all(int pid);
int slow_computation(int pid);
int regular_computation(int pid);
