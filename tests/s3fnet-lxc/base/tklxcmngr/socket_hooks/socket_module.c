#include "socket_module.h"
#include "hook_defs.h"

/*
Has basic functionality for the Kernel Module itself. It defines how the userland process communicates with the kernel module,
as well as what should happen when the kernel module is initialized and removed.
*/




int proc_num; // number of LXCs in the experiment
struct mutex exp_mutex; // experiment mutex to prevent race conditions

int experiment_stopped; // flag to determine state of the experiment
struct list_head exp_list; // linked list of all tasks in the experiment
struct lxc_entry * lxc_list_head;
struct lxc_entry * lxc_list_tail;

// Proc file declarations
static struct proc_dir_entry *hook_dir;
static struct proc_dir_entry *hook_file;
struct proc_dir_entry *lxc_dir;

unsigned long **sys_call_table; //address of the sys_call_table, so we can hijack certain system calls

int TOTAL_CPUS; //number of CPUs in the system

unsigned long original_cr0; //The register to hijack sys_call_table
char lxcBuff[KERNEL_BUF_SIZE];
char curr_lxcName[100] = "None";


/*
Gets the PID of our spinner task (only in 64 bit)
*/
/*
int getSpinnerPid(struct subprocess_info *info, struct cred *new) {
        loop_task = current;
        return 0;
}
*.
/*
Hack to get 64 bit running correctly. Starts a process that will just loop while the experiment is going
on. Starts an executable specified in the path in user space from kernel space. Currently removed. Look at it later ****
*/
/*
int call_usermodehelper_dil(char *path, char **argv, char **envp, int wait)
{
	struct subprocess_info *info;
        gfp_t gfp_mask = (wait == UMH_NO_WAIT) ? GFP_ATOMIC : GFP_KERNEL;

        info = call_usermodehelper_setup(path, argv, envp, gfp_mask, getSpinnerPid, NULL, NULL);
        if (info == NULL)
                return -ENOMEM;

        return call_usermodehelper_exec(info, wait);
}
*/
/***
This handles how a process from userland communicates with the kernel module. The process basically writes to:
/proc/send_hook/send_hook_command with a command
***/
ssize_t status_write(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char write_buffer[KERNEL_BUF_SIZE];
	unsigned long buffer_size;
	char * lxcname_end;
	int no_of_bytes = 0;
	printk(KERN_INFO "Socket Hook: Received a write\n");

 	if(count > KERNEL_BUF_SIZE)
	{
    		buffer_size = KERNEL_BUF_SIZE;
  	}
	else
	{
		buffer_size = count;
	}

  	if(copy_from_user(write_buffer, buffer, buffer_size))
	{
	    return -EFAULT;
	}
	
	// Use +2 to skip over the first two characters (i.e. the switch and the ,)
	switch(write_buffer[0]){
		
		case ADD_COMMAND	: create_new_lxc_entry(write_buffer + 2);
					  printk(KERN_INFO "Socket Hook : Added new lxc entry\n");
					  break;

		case START_COMMAND	: experiment_stopped = 0;
					  printk(KERN_INFO "Socket Hook : Started Experiment\n");
					  break;

		case STOP_COMMAND	: experiment_stopped = 1;
					  printk(KERN_INFO "Socket Hook : Stopped Experiment\n");
					  remove_all_lxc_entries();
					  break;
		case LOAD_COMMAND	: lxcname_end = write_buffer + 2;
					  while(*lxcname_end != ','){
						no_of_bytes ++;		
						lxcname_end ++;
				          }	
					  flush_buffer(curr_lxcName,100);
					  memcpy(curr_lxcName,write_buffer + 2,no_of_bytes);
					  //printk(KERN_INFO "Socket Hook : Load command %s\n",curr_lxcName);
					  break;
		default			: printk(KERN_INFO "Socket Hook : Invalid command\n");
					  break;
	
	}
	return count;
}

/***
This function gets executed when the kernel module is loaded. It creates the file for process -> kernel module communication,
sets up mutexes, timers, and hooks the system call table.
***/
int __init my_module_init(void)
{
	int i;

   	printk(KERN_INFO "Socket Hook : Loading Socket Hook MODULE\n");

	//Set up TimeKeeper status file in /proc
  	hook_dir = proc_mkdir(HOOK_DIR, NULL);
  	if(hook_dir == NULL)
	{
	    	remove_proc_entry(HOOK_DIR, NULL);
   		printk(KERN_INFO "Socket Hook: Error: Could not initialize /proc/%s\n", HOOK_DIR);
   		return -ENOMEM;
  	}
  	printk(KERN_INFO "Socket Hook: /proc/%s created\n", HOOK_DIR);
	hook_file = proc_create(HOOK_FILE, 0660, hook_dir,&proc_file_fops);
	if(hook_file == NULL)
	{
	    	remove_proc_entry(HOOK_FILE, hook_dir);
   		printk(KERN_ALERT "Error: Could not initialize /proc/%s/%s\n", HOOK_DIR, HOOK_FILE);
   		return -ENOMEM;
  	}
	printk(KERN_INFO "Socket Hook: /proc/%s/%s created\n", HOOK_DIR, HOOK_FILE);
	

	lxc_dir = proc_mkdir(LXC_DIR, NULL);
	if(lxc_dir == NULL)
	{
		remove_proc_entry(LXC_DIR, NULL);
		printk(KERN_ALERT "Socket Hook: Error: Could not initialize /proc/%s\n",LXC_DIR);
		return -ENOMEM;
	}
	printk(KERN_INFO "Socket Hook: /proc/%s created\n", LXC_DIR);


	experiment_stopped = 1;
	lxc_list_head = NULL;
	lxc_list_tail = NULL;
	proc_num = 0;
	//mutex_init(&exp_mutex);
	
	printk(KERN_INFO "Socket Hook: Acquiring system call table\n");
	//Acquire sys_call_table, hook system calls
        if(!(sys_call_table = aquire_sys_call_table())){
		printk(KERN_INFO "Socket Hook: Error acquiring system call table\n");
                return -1;
	}
	original_cr0 = read_cr0();
	write_cr0(original_cr0 & ~0x00010000);
	#ifndef __x86_64
		ref_socketcall = (void *)sys_call_table[__NR_socketcall];
	        sys_call_table[__NR_socketcall] = (unsigned long *)new_socketcall;
	#else
		ref_sendmsg = (void *)sys_call_table[__NR_sendmsg];
		sys_call_table[__NR_sendmsg] = (unsigned long *)new_sendmsg;
		
		ref_sendto = (void*)sys_call_table[__NR_sendto];
		sys_call_table[__NR_sendto] = (unsigned long *)new_sendto;
	#endif	
	write_cr0(original_cr0);

	printk(KERN_INFO "Socket Hook: System call table hooked. Initialization successfull\n");

  	return 0;
}

/***
This function gets called when the kernel module is unloaded. It frees up all memory, deletes timers, and fixes
the system call table.
***/
void __exit my_module_exit(void)
{
	int i;

	//clean_exp();
	
	remove_proc_entry(HOOK_FILE, hook_dir);
   	printk(KERN_INFO "Socket Hook: /proc/%s/%s deleted\n", HOOK_DIR, HOOK_FILE);
   	remove_proc_entry(HOOK_DIR, NULL);
   	printk(KERN_INFO "Socket Hook: /proc/%s deleted\n", HOOK_DIR);


	//Fix sys_call_table
        if(!sys_call_table)
                return;
	write_cr0(original_cr0 & ~0x00010000);
	#ifndef __x86_64
        sys_call_table[__NR_socketcall] = (unsigned long *)ref_socketcall;
	#else
	sys_call_table[__NR_sendmsg] = (unsigned long *)ref_sendmsg;
	sys_call_table[__NR_sendto] = (unsigned long *)ref_sendto;
	#endif
	
	write_cr0(original_cr0);
	experiment_stopped = 1;
	remove_all_lxc_entries();
	remove_proc_entry(LXC_DIR, NULL);
   	printk(KERN_INFO "Socket Hook: /proc/%s deleted\n", LXC_DIR);
	
}

//needs to be defined, but we do not read from /proc/send_hook/send_hook_command so we do not do anything here
ssize_t status_read(struct file *pfil, char __user *pBuf, size_t len, loff_t *p_off)
{
        static int finished = 0,i = 0;
	int buffer_size = 0;
	char NULL_msg[5] = "NULL";
	char tmpBuff[KERNEL_BUF_SIZE];
	unsigned long flags;
	int j = 0;
	for(j = 0; j < KERNEL_BUF_SIZE; j++)
		tmpBuff[j] = NULL;
	struct lxc_entry * lxc;
	if ( finished ) {
		printk(KERN_INFO "Socket Hook : Proc file read: END\n");
		finished = 0;
		return 0;
	}
	finished = 1;

	lxc = NULL;
	
	
	lxc = get_lxc_entry(curr_lxcName);	


	if(lxc == NULL){ // Specified LXC does not exist.
		if (copy_to_user(pBuf, NULL_msg, 4) ) {
			return -EFAULT;
		}		
		return 4;
	}

	spin_lock_irqsave(&lxc->lxc_entry_lock,flags);
	if(lxc->is_buf_initialised == 0){
		spin_unlock_irqrestore(&lxc->lxc_entry_lock,flags);
		if (copy_to_user(pBuf, NULL_msg, 4) ) {
			
			return -EFAULT;
		}		
		return 4;
	}
	while(lxc->lxcBuff[i] != NULL)
		i++;
	buffer_size = i;
	if(buffer_size > 0 && buffer_size < KERNEL_BUF_SIZE)
		strncpy(tmpBuff,lxc->lxcBuff,buffer_size);
	for(i = 0; i < KERNEL_BUF_SIZE; i++){
		lxc->lxcBuff[i] = '\0'	;
	}
	spin_unlock_irqrestore(&lxc->lxc_entry_lock,flags);


	

	
	if(buffer_size == 0 || buffer_size >= KERNEL_BUF_SIZE){
		if (copy_to_user(pBuf, NULL_msg, 4) ) {
			return -EFAULT;
		}		
		return 4;
	}
	if (copy_to_user(pBuf, tmpBuff, buffer_size) ) {
		return -EFAULT;
	}


	
	return buffer_size; /* Return the number of bytes "read" */
	
}


// Register the init and exit functions here so insmod can run them
module_init(my_module_init);
module_exit(my_module_exit);

// Required by kernel
MODULE_LICENSE("GPL");
