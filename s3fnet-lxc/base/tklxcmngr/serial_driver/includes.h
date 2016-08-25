
#ifndef __INCLUDES_H

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/proc_fs.h>
#include <linux/fcntl.h>	/* O_ACCMODE */
#include <linux/seq_file.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/sched.h>
#include <linux/cgroup.h>

//#include <asm/system.h>		/* cli(), *_flags */
#include <asm/uaccess.h>	/* copy_*_user */
#include "../../s3fnet-definitions.h"
#include "hashmap.h"

typedef unsigned int uint32_t;

/*
 * Ioctl definitions
 */

/* Use 'k' as magic number */
#define S3FSERIAL_IOC_MAGIC  'k'

#define S3FSERIAL_IOWRX _IOW(S3FSERIAL_IOC_MAGIC,  1, int) // write to the rx buffer from user space. used by S3F simulator.
#define S3FSERIAL_IORTX _IOR(S3FSERIAL_IOC_MAGIC,  2, int) // read the tx buffer into user space. used by S3F simulator.
#define S3FSERIAL_SETCONNLXC _IOW(S3FSERIAL_IOC_MAGIC,  3, int) // set the dest_lxc_name for conneciton passed as param. used by AWLSIM
#define S3FSERIAL_GETCONNLXC _IOR(S3FSERIAL_IOC_MAGIC,  4, int) // get the dest_lxc_name for connection passed as param. used by S3F
#define S3FSERIAL_GETCONNID _IOR(S3FSERIAL_IOC_MAGIC,  5, int) // get the CONN_ID for connection SPECIFIED BY TWO LXCS as param. used by S3F
#define S3FSERIAL_GETCONNSTATUS _IOR(S3FSERIAL_IOC_MAGIC, 6, int)
#define S3FSERIAL_GETACTIVECONNS _IOR(S3FSERIAL_IOC_MAGIC,7,int)

#endif