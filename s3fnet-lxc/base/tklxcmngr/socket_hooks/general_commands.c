#include "socket_module.h"

/*
Contains some general commands that a are used by Socket Hook module
*/




int get_next_index(char * buffer);
char * extract_filename(char *str);
void get_lxc_name(struct task_struct * process, char * lxcName);
struct lxc_entry * get_lxc_entry(char * lxcName);
int packet_hash(u_char * s,int size);
void flush_buffer(char * buffer, int size);
int write_new_timestamp(char * lxcName, long hash, struct timeval tv);
struct task_struct * get_init_task_struct(struct task_struct * current_process);
int handle_new_message(struct task_struct * current_process,char * msg,int size, struct timeval tv);
extern struct mutex cgroup_mutex;
static struct proc_dir_entry *lxc_file;

/***
 Convert string to integer
***/
int atoi(char *s)
{
        int i,n;
        n = 0;
        for(i = 0; *(s+i) >= '0' && *(s+i) <= '9'; i++)
                n = 10*n + *(s+i) - '0';
        return n;
}

void replace_special_chars(char * s){
	int i,n;
	while(*(s+i) != '\0'){
		if((*(s+i) >= '0' && *(s+i) <= '9') || (*(s+i) >= 'a' && *(s+i) <= 'z') || (*(s+i) >= 'A' && *(s+i) <= 'Z'))
			;
		else
			*(s+i) = '0';
		i++;
	}

}

void create_new_lxc_entry(char * buffer){
	struct lxc_entry * new_lxc;
	int PID,nxt_index;
	int no_of_bytes = 0;
	char * lxc_name_end;
	char * lxcName;
	PID = atoi(buffer);
	nxt_index = get_next_index(buffer);
	lxcName = (char*)(buffer + nxt_index);
	lxc_name_end = buffer + nxt_index;
	while(*lxc_name_end != ','){
		lxc_name_end ++;
		no_of_bytes ++;
	}
	//no_of_bytes -= 1;
	if(proc_num == 0){
		lxc_list_head = (struct lxc_entry *)kmalloc(sizeof(struct lxc_entry),GFP_KERNEL);
		lxc_list_head->is_buf_initialised = 0;
		spin_lock_init(&lxc_list_head->lxc_entry_lock);
		lxc_list_head->next = NULL;
		lxc_list_head->prev = NULL;
		lxc_list_head->PID = PID;
		lxc_list_tail = lxc_list_head;
		flush_buffer(lxc_list_head->lxcName,100);
		memcpy(lxc_list_head->lxcName,lxcName,no_of_bytes);

		//replace_special_chars(lxc_list_head->lxcName);
		printk(KERN_INFO "Socket Hook : New lxc entry name %s, PID = %d, No of bytes = %d \n", lxc_list_head->lxcName,PID,no_of_bytes);
		
		proc_num += 1;
	}
	else {
		
		new_lxc = (struct lxc_entry *)kmalloc(sizeof(struct lxc_entry),GFP_KERNEL);
		new_lxc->PID = PID;
		flush_buffer(new_lxc->lxcName,100);
		memcpy(new_lxc->lxcName,lxcName,no_of_bytes);
		//replace_special_chars(new_lxc->lxcName);
		printk(KERN_INFO "Socket Hook : New lxc entry name %s, PID = %d, No of bytes = %d\n", new_lxc->lxcName,PID,no_of_bytes);
		new_lxc->is_buf_initialised = 0;
		spin_lock_init(&new_lxc->lxc_entry_lock);
		lxc_list_tail->next = new_lxc;
		new_lxc->prev = lxc_list_tail;
		new_lxc->next = NULL;
		
		lxc_list_tail = new_lxc;
		proc_num += 1;
		
	}



}

void remove_all_lxc_entries(){
	struct lxc_entry * curr_lxc;
	while(lxc_list_head != NULL){
		
		curr_lxc = lxc_list_head;
		lxc_list_head = lxc_list_head->next;
		kfree(curr_lxc);		

	}
	lxc_list_tail = NULL;

}

int get_next_index(char * buffer){

	int i = 0;
	while(*(buffer + i) != ' ')
		i++;
	return i + 1;	

}

char * extract_filename(char *str)
{
    int     ch = '/';
    char   *pdest;

 
    // Search backwards for last backslash in filepath 
    pdest = strrchr(str, ch);
     
    // if backslash not found in filepath
    if(pdest == NULL )
    {
        pdest = str;  // The whole name is a file in current path? 
    }
    else
    {
    	pdest++; // Skip the backslash itself.
    }
     
    // extract filename from file path
    return pdest;
}

void get_lxc_name(struct task_struct * process, char * lxcName){

	struct cgroup * cgrp;

	char buf[KERNEL_BUF_SIZE];
	struct cgroupfs_root *root;
	int retval = 0;
	char * name;
	
	
	
	cgrp = task_cgroup(process,1);
	if(cgrp != NULL){
		retval = cgroup_path(cgrp, buf, KERNEL_BUF_SIZE);
		
		if (retval < 0){
			strcpy(lxcName,"NA");
			return;
		}
		else{
			if(strcmp(buf,"/") == 0)
					;
		else{
				name = extract_filename(buf);
				strcpy(lxcName,name);
				return;	

			}
		}
	}
	else{
		//printk(KERN_INFO "Socket Hook : Task cgroup is NULL\n");
	}

	strcpy(lxcName,"NA");
	

}

struct lxc_entry * get_lxc_entry(char * lxcName){

	struct lxc_entry * head;
	struct lxc_entry * tail;
	struct lxc_entry * lxc;
	head = lxc_list_head;
	tail = lxc_list_tail;
	
	lxc = NULL;

	while(head != NULL){
		if(strcmp(head->lxcName,lxcName) == 0)
			return head;
		else
			head = head->next;

	}

	return lxc;

}

int packet_hash(u_char * s,int size)
{

    //http://stackoverflow.com/questions/114085/fast-string-hashing-algorithm-with-low-collision-rates-with-32-bit-integer
    int hash = 0;
    u_char * ptr = s;
    int i = 0;
    
    for(i = 0; i < size; i++)
    {
    	hash += *ptr;
    	hash += (hash << 10);
    	hash ^= (hash >> 6);

        ++ptr;
    }

    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);


    return hash;
}


void flush_buffer(char * buffer, int size){
	int i = 0;
	for(i = 0; i < size; i++)
		buffer[i] = NULL;
	

}

int write_new_timestamp(char * lxcName, long hash, struct timeval tv){

	struct lxc_entry * lxc;
	unsigned long flags;
	lxc = get_lxc_entry(lxcName);
	if(lxc == NULL){
		printk(KERN_INFO "Socket Hook: LXC does not exist\n");
		return -1;
	}
	printk(KERN_INFO "Socket Hook : Writing new timestamp for lxc %s, secs : %lu, usec : %lu\n",lxcName,tv.tv_sec,tv.tv_usec);
	spin_lock_irqsave(&lxc->lxc_entry_lock,flags);
	flush_buffer(lxc->lxcBuff,KERNEL_BUF_SIZE);	
	sprintf(lxc->lxcBuff,"%lu\n%lu\n%lu\n",tv.tv_sec,tv.tv_usec,1234);
	lxc->is_buf_initialised = 1;
	spin_unlock_irqrestore(&lxc->lxc_entry_lock,flags);

	
	return 1;
	
}

struct task_struct * get_init_task_struct(struct task_struct * current_process){
    char lxcName[100];
    struct task_struct *task = current_process; // getting global current pointer
    do
    {
	printk(KERN_INFO "Socket Hook : PID : %d, comm = %s\n",task->pid,task->comm);
	get_lxc_name(task,lxcName);
	flush_buffer(lxcName,100);
        task = task->parent;
	
    
    } while (task->pid != 1);
    printk(KERN_INFO "Socket Hook : PID : %d, comm = %s\n",task->pid,task->comm);
    printk(KERN_INFO "Socket Hook : Got init task struct\n");
    return task;
}

int handle_new_message(struct task_struct * current_process,char * msg,int size, struct timeval tv){

	struct task_struct * init_task;
	char lxcName[KERNEL_BUF_SIZE];
	long hash;
	hash = (long) packet_hash(msg,size);
	get_lxc_name(current_process,lxcName);
	return write_new_timestamp(lxcName,hash,tv);
	

}




