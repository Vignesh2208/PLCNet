#include "includes.h"

void flush_buffer(char *a, int size){
	int i = 0;
	for(i = 0; i< size; i++)
		a[i] = '\0';

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

int get_fd(struct file * filp){
	int fd = -1;
	struct files_struct * fs;
	int max_fds = 0, i = 0;
	rcu_read_lock();
	fs = current->files;
	max_fds = fs->max_fds;
	for(i = 0; i < max_fds; i++){
		if(filp == fs[i]){
			fd = i;
			break;
		}
	}
	rcu_read_unlock();
	return fd;

}

/* Utility functions  a % b*/
int mod(uint32_t a, uint32_t b){


  while(a < 0){
    a = a + b;
  }
  return (a - (a/b)*b);
}

void get_lxc_name(struct task_struct * process, char * lxcName){

	struct cgroup * cgrp;

	char buf[KERN_BUF_SIZE];
	struct cgroupfs_root *root;
	int retval = 0;
	char * name;
	
	
	
	cgrp = task_cgroup(process,1);
	if(cgrp != NULL){
		retval = cgroup_path(cgrp, buf, KERN_BUF_SIZE);
		
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

void copy_bytes(char * src_buffer, char * dst_buffer, uint32_t buf_size, uint32_t start_pos, uint32_t num_bytes){

	int i = 0;
	int num_copied = 0;
	if(buf_size > 0 && num_bytes <= buf_size && num_bytes > 0){
		flush_buffer(dst_buffer,buf_size);
		i = start_pos;
		while(num_copied < num_bytes){			
			dst_buffer[num_copied] = src_buffer[mod(i,buf_size)];
			i++;
			num_copied++;
		}

	}

}