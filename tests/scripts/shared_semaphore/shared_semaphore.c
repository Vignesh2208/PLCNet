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
#include <stdio.h>
#include <semaphore.h>
#include <sys/uio.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/mman.h>
#include <time.h>


typedef struct sharedsem
{
 sem_t mutex;
}shared_sem;

shared_sem * mmap_sem [3];


static PyObject * init_shared_sem_func(PyObject * self, PyObject * args){
  int sem_no;
  int conn_id;
  int should_initialize = 0;
  char f_name[100];

  if (!PyArg_ParseTuple(args, "iii", &sem_no, &conn_id, &should_initialize)) {
        Py_RETURN_NONE;
  }

  sprintf(f_name,"test%d", sem_no);

  //shm_unlink(f_name);
  int fd = shm_open(f_name,O_CREAT|O_RDWR,S_IRUSR | S_IWUSR);
  if(fd == -1){
    printf("Error in open\n");
    Py_RETURN_NONE; 
  }


  ftruncate(fd,sizeof(shared_sem));
  mmap_sem[conn_id] = mmap(NULL,sizeof(shared_sem),PROT_READ|PROT_WRITE,MAP_SHARED,fd, 0);
  close(fd);

  if(should_initialize)
    sem_init(&mmap_sem[conn_id]->mutex, 1, 1); 

  Py_RETURN_NONE; 

}

static PyObject *acquire_shared_sem_func(PyObject *self, PyObject *args) {

  int conn_id;

  if (!PyArg_ParseTuple(args, "i", &conn_id)) {
        return Py_BuildValue("i", 0); // Failed
  }

  if(sem_trywait(&mmap_sem[conn_id]->mutex) == 0){

    //printf("Sem wait succeeded\n");
    return Py_BuildValue("i", 1); // Success

  }

  return Py_BuildValue("i", 0); // Failed

}

static PyObject * release_shared_sem_func(PyObject *self, PyObject *args) {

  int conn_id;
  if (!PyArg_ParseTuple(args, "i", &conn_id)) {
        return Py_BuildValue("i", 0); // Failed
  }

  sem_post(&mmap_sem[conn_id]->mutex);
 

  return Py_BuildValue("i", 1); // Success

}


static PyMethodDef shared_sem_methods[] = {
   { "init_shared_sem", init_shared_sem_func, METH_VARARGS, NULL },
   { "acquire_shared_sem", acquire_shared_sem_func, METH_VARARGS, NULL },
   { "release_shared_sem", release_shared_sem_func, METH_VARARGS, NULL }
};





#if PY_MAJOR_VERSION <= 2

void initshared_sem(void)
{
    Py_InitModule3("shared_sem", shared_sem_methods,
                   "shared semaphore");
}

#elif PY_MAJOR_VERSION >= 3 

static struct PyModuleDef shared_sem_definition = { 
    PyModuleDef_HEAD_INIT,
    "shared_sem",
    "A Python module that mmap's a shared_sem between 2 processes",
    -1, 
    shared_sem_methods
};
PyMODINIT_FUNC PyInit_shared_sem(void)
{
    Py_Initialize();

    return PyModule_Create(&shared_sem_definition);
}

#endif
