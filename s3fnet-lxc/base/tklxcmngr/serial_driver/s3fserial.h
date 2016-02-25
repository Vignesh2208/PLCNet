#ifndef __MAIN_MODULE_H

#include "includes.h"

typedef unsigned char uint8_t;

struct lxc_connection
{
	uint8_t params_set;
	char tx_buf[TX_BUF_SIZE];
	char rx_buf[RX_BUF_SIZE];
	char dst_lxc_name[KERN_BUF_SIZE];
	int num_bytes_to_transmit;
	int num_received_bytes;
	uint32_t recv_start;
	spinlock_t conn_lock;

};

struct dev_struct {

	hashmap lxcs;	
	uint32_t id;		// device id
	spinlock_t dev_lock;
	struct semaphore sem;     /* mutual exclusion semaphore     */
	struct cdev cdev;	  /* Char device structure		*/
};


struct lxc_entry {

	int PID;
	char lxcName[100];
	struct lxc_connection * connection;
	struct dev_struct * dev;

};


struct ioctl_conn_param{

	int conn_id;
	char owner_lxc_name[KERN_BUF_SIZE];	
	char dst_lxc_name[KERN_BUF_SIZE];
	int num_bytes_to_write;			 // number of bytes to write to rxbuf
	char bytes_to_write[RX_BUF_SIZE];// buffer from which data is copied to lxc's rx_buf
	int num_bytes_to_read;			 // number of bytes to read from txbuf
	char bytes_to_read[TX_BUF_SIZE]; // buffer to which data from txbuf is copied.
};





ssize_t s3fserial_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
ssize_t s3fserial_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
loff_t  s3fserial_llseek(struct file *filp, loff_t off, int whence);
long    s3fserial_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
int s3fserial_open(struct inode *inode, struct file *filp);
int s3fserial_release(struct inode *inode, struct file *filp);
uint32_t s3fserial_poll(struct file *filp, poll_table *wait);
int init_s3fserial(void);
int cleanup_s3fserial(void);

extern void flush_buffer(char *a, int size);
extern char * extract_filename(char *str);
extern int get_fd(struct file * filp);
extern void get_lxc_name(struct task_struct * process, char * lxcName);
extern int mod(uint32_t a, uint32_t b);
extern void copy_bytes(char * src_buffer, char * dst_buffer, uint32_t buf_size, uint32_t start_pos, uint32_t num_bytes);


#endif
