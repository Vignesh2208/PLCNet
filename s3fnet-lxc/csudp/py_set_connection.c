#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h> 
#include <sys/ioctl.h>
#include <fcntl.h>
#include <Python.h>



#define NR_SERIAL_DEVS        3
#define NR_DEVS 1
#define KERN_BUF_SIZE 100
#define TX_BUF_SIZE 100
#define RX_BUF_SIZE 2*(TX_BUF_SIZE)


#define S3FSERIAL_IOC_MAGIC  'k'
#define S3FSERIAL_IOWRX _IOW(S3FSERIAL_IOC_MAGIC,  1, int) // write to the rx buffer from user space. used by S3F simulator.
#define S3FSERIAL_IORTX _IOR(S3FSERIAL_IOC_MAGIC,  2, int) // read the tx buffer into user space. used by S3F simulator.
#define S3FSERIAL_SETCONNLXC _IOW(S3FSERIAL_IOC_MAGIC,  3, int) // set the dest_lxc_name for conneciton passed as param. used by AWLSIM
#define S3FSERIAL_GETCONNLXC _IOR(S3FSERIAL_IOC_MAGIC,  4, int) // get the dest_lxc_name for connection passed as param. used by S3F
#define S3FSERIAL_GETCONNID _IOR(S3FSERIAL_IOC_MAGIC,  5, int) // get the CONN_ID for connection SPECIFIED BY TWO LXCS as param. used by S3F
#define S3FSERIAL_GETCONNSTATUS _IOR(S3FSERIAL_IOC_MAGIC, 6, int)
#define S3FSERIAL_GETACTIVECONNS _IOR(S3FSERIAL_IOC_MAGIC,7,int)


struct ioctl_conn_param{

	int conn_id;						
	char owner_lxc_name[KERN_BUF_SIZE];	
	char dst_lxc_name[KERN_BUF_SIZE];
	int num_bytes_to_write;			 // number of bytes to write to rxbuf
	char bytes_to_write[RX_BUF_SIZE];// buffer from which data is copied to lxc's rx_buf
	int num_bytes_to_read;			 // number of bytes to read from txbuf
	char bytes_to_read[TX_BUF_SIZE]; // buffer to which data from txbuf is copied.
};

void flush_buffer(char * buf, int size){
	int i = 0;
	for(i = 0; i < size; i++)
		buf[i] = '\0';
}

void reset_ioctl_conn(struct ioctl_conn_param * ioctl_conn){
	flush_buffer(ioctl_conn->owner_lxc_name,KERN_BUF_SIZE);
	flush_buffer(ioctl_conn->dst_lxc_name,KERN_BUF_SIZE);
  	flush_buffer(ioctl_conn->bytes_to_write,RX_BUF_SIZE);
  	flush_buffer(ioctl_conn->bytes_to_read,TX_BUF_SIZE);
  	ioctl_conn->conn_id = 0;
  	ioctl_conn->num_bytes_to_write = 0;
  	ioctl_conn->num_bytes_to_read = 0;
}



static PyObject *set_connection_func(PyObject *self, PyObject *args) {

   	int src_lxc_id;
   	int dst_lxc_id;
   	int conn_id;
   	int fd;

   	char src_lxc_name[KERN_BUF_SIZE];
   	char dst_lxc_name[KERN_BUF_SIZE];

   	struct ioctl_conn_param ioctl_conn;

   	if (!PyArg_ParseTuple(args, "iii", &src_lxc_id, &dst_lxc_id, &conn_id)) {
      	Py_RETURN_NONE;
   	}

   	reset_ioctl_conn(&ioctl_conn);
   	flush_buffer(src_lxc_name,KERN_BUF_SIZE);
   	flush_buffer(dst_lxc_name,KERN_BUF_SIZE);
   	sprintf(src_lxc_name,"lxc%d-0",src_lxc_id);
   	sprintf(dst_lxc_name,"lxc%d-0",dst_lxc_id);

   	strcpy(ioctl_conn.owner_lxc_name,src_lxc_name);
   	ioctl_conn.conn_id = conn_id;
   	strcpy(ioctl_conn.dst_lxc_name,dst_lxc_name);

   	printf("src lxc name = %s, dst lxc name = %s, conn_id = %d\n",src_lxc_name,dst_lxc_name,conn_id);
   	fflush(stdout);

	fd = open("/dev/s3fserial0",O_RDWR);
	if(ioctl(fd,S3FSERIAL_SETCONNLXC,&ioctl_conn) < 0){
		fprintf(stdout,"Client : Ioctl SETCONNLXC error\n");
		fflush(stdout);
		close(fd);
		Py_RETURN_NONE;
	}

	close(fd);
   
   /* Do something interesting here. */
   Py_RETURN_NONE;
}

static PyMethodDef connection_methods[] = {
   { "set_connection", set_connection_func, METH_VARARGS, NULL },
};





void initset_connection(void)
{
    Py_InitModule3("set_connection", connection_methods,
                   "Extension module example!");
}