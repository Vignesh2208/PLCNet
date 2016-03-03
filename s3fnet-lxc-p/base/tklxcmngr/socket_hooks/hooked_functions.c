#include "socket_module.h"


/*
Contains the code for acquiring the syscall table, as well as the sendmsg system call hooks.
*/

unsigned long **aquire_sys_call_table(void);

#ifndef __x86_64
asmlinkage ssize_t new_socketcall(int call, unsigned long * args);
asmlinkage ssize_t (*ref_socketcall)(int call, unsigned long * args);
#else
asmlinkage ssize_t new_sendmsg(int sockfd, const struct msghdr *msg, int flags);
asmlinkage ssize_t (*ref_sendmsg)(int sockfd, const struct msghdr *msg, int flags);

asmlinkage ssize_t new_send(int sockfd, const void *buf, size_t len, int flags);
asmlinkage ssize_t (*ref_send)(int sockfd, const void *buf, size_t len, int flags);

asmlinkage ssize_t new_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, int addrlen);
asmlinkage ssize_t (*ref_sendto)(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, int addrlen);
#endif


extern int experiment_stopped;
extern int handle_new_message(struct task_struct * tsk, char *  msg, int size, struct timeval tv);

void get_dilated_time(struct task_struct * task,struct timeval* tv)
{
	s64 temp_past_physical_time;

	do_gettimeofday(tv);
	if(task->virt_start_time != 0){
		if (task->group_leader != task) { //use virtual time of the leader thread
                       	task = task->group_leader;
               	}
		s64 now = timeval_to_ns(tv);
		s32 rem;
		s64 real_running_time;
		s64 dilated_running_time;
		real_running_time = now - task->virt_start_time;
		if (task->freeze_time != 0)
			temp_past_physical_time = task->past_physical_time + (now - task->freeze_time);
		else
			temp_past_physical_time = task->past_physical_time;

		if (task->dilation_factor > 0) {
			dilated_running_time = div_s64_rem( (real_running_time - temp_past_physical_time)*1000 ,task->dilation_factor,&rem) + task->past_virtual_time;
			now = dilated_running_time + task->virt_start_time;
		}
		else if (task->dilation_factor < 0) {
			dilated_running_time = div_s64_rem( (real_running_time - temp_past_physical_time)*(task->dilation_factor*-1),1000,&rem) + task->past_virtual_time;
			now =  dilated_running_time + task->virt_start_time;
		}
		else {
			dilated_running_time = (real_running_time - temp_past_physical_time) + task->past_virtual_time;
			now = dilated_running_time + task->virt_start_time;
		}
		*tv = ns_to_timeval(now);
	}

	return;

}



//extern asmlinkage long gettimepid(pid_t pid, struct timeval __user *tv, struct timezone __user *tz);
#ifndef __x86_64
/*
Hooks the socketcall system call
*/
asmlinkage ssize_t new_socketcall(int call, unsigned long * args){
	char msg[100] = "Hello";
	struct timeval tv;
	
	
	get_dilated_time(current,&tv);
	
	if (experiment_stopped == 0 && current->virt_start_time != NOTSET)
	{
		
		if(call == SYS_SEND || call == SYS_SENDTO || call == SYS_SENDMSG || call == SYS_SENDMMSG){
		
			handle_new_message(current,msg,5,tv);
		}

	}
        	
        return ref_socketcall(call,args);
}



#else
/*
Hooks the sendmsg system call
*/
asmlinkage ssize_t new_sendmsg(int sockfd, const struct msghdr *msg, int flags) {
	char new_msg[100] = "Hello";
	struct timeval tv;
	get_dilated_time(current,&tv);
	        
	if (experiment_stopped == 0 && current->virt_start_time != NOTSET)
	{
		handle_new_message(current,new_msg,5,tv);

	}
        	
        return ref_sendmsg(sockfd,msg,flags);
}

// Not hooked. Does not exist in system call table.
asmlinkage ssize_t new_send(int sockfd, const void *buf, size_t len, int flags){

	char msg[100] = "Hello";
	struct timeval tv;
	get_dilated_time(current,&tv);
	        
	if (experiment_stopped == 0 && current->virt_start_time != NOTSET)
	{
		handle_new_message(current,msg,5,tv);

	}
        	
        return ref_send(sockfd,buf,len,flags);

}

asmlinkage ssize_t new_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, int addrlen){

	char msg[100] = "Hello";
	
	struct timeval tv;
	get_dilated_time(current,&tv);
	        
	if (experiment_stopped == 0 && current->virt_start_time != NOTSET)
	{
		handle_new_message(current,msg,5,tv);

	}
        	
        return ref_sendto(sockfd,buf,len,flags,dest_addr,addrlen);



}

#endif


/***
Finds us the location of the system call table
***/
unsigned long **aquire_sys_call_table(void)
{
        unsigned long int offset = PAGE_OFFSET;
        unsigned long **sct;
        while (offset < ULLONG_MAX) {
                sct = (unsigned long **)offset;

                if (sct[__NR_close] == (unsigned long *) sys_close)
                        return sct;

                offset += sizeof(void *);
        }
        return NULL;
}

