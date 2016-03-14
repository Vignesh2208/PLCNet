#ifndef HEADER_FILE
#define HEADER_FILE

#define LINUX

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <asm/uaccess.h>
#include <linux/signal.h>
#include <linux/syscalls.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <linux/netdevice.h>
#include <asm/siginfo.h>
#include <asm/unistd.h>
#include <net/sock.h>
#include <linux/socket.h>

#include <linux/netlink.h>
#include <linux/skbuff.h>
#include <linux/spinlock_types.h>
#include <linux/cgroup.h>
#include "hook_defs.h"
#include <linux/utsname.h>
#include <asm/pgtable.h>
#include <linux/spinlock.h>
#include <linux/fdtable.h>

// the callback functions for the Socket Hook Command file
ssize_t status_read(struct file *pfil, char __user *pBuf, size_t len, loff_t *p_off);
ssize_t status_write(struct file *file, const char __user *buffer, size_t count, loff_t *data);


static const struct file_operations proc_file_fops = {
 .read = status_read,
 .write = status_write,
};

struct lxc_entry
{
    int PID;
	char lxcName[100];
	char lxcBuff[KERNEL_BUF_SIZE];
	spinlock_t lxc_entry_lock;
	int is_buf_initialised;
    struct dilation_task_struct *next; // the next dilation_task_struct in the per cpu chain
    struct dilation_task_struct *prev; // the prev dilation_task_struct in the per cpu chain
	struct proc_dir_entry *lxc_file;
        
        
};


extern struct proc_dir_entry *lxc_dir;


// *** hooked_functions.c
extern unsigned long **aquire_sys_call_table(void);
#ifndef __x86_64
extern asmlinkage int new_socketcall(int call, unsigned long * args) ;
extern asmlinkage int (*ref_socketcall)(int call, unsigned long *args);
#else
extern asmlinkage ssize_t new_sendmsg(int sockfd, const struct msghdr *msg, int flags) ;
extern asmlinkage ssize_t (*ref_sendmsg)(int sockfd, const struct msghdr *msg, int flags);

extern asmlinkage ssize_t new_send(int sockfd, const void *buf, size_t len, int flags);
extern asmlinkage ssize_t (*ref_send)(int sockfd, const void *buf, size_t len, int flags);

extern asmlinkage ssize_t new_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, int addrlen);
extern asmlinkage ssize_t (*ref_sendto)(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, int addrlen);

#endif



extern void create_new_lxc_entry(char * buffer);
extern int atoi(char *s);
extern int handle_new_message(struct task_struct * tsk, char *  msg, int size, struct timeval tv);
extern int proc_num;
extern struct lxc_entry * lxc_list_head;
extern struct lxc_entry * lxc_list_tail;
extern int experiment_stopped; // flag to determine state of the experiment
extern struct list_head exp_list; // linked list of all tasks in the experiment
extern struct mutex exp_mutex; // experiment mutex to prevent race conditions
extern void remove_all_lxc_entries(void);
extern char * extract_filename(char *str);
extern struct lxc_entry * get_lxc_entry(char * lxcName);
extern void flush_buffer(char * buffer, int size);


#endif

