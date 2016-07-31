EXTRA_CFLAGS += -w
KERNEL_SRC:= /lib/modules/$(shell uname -r)/build
SUBDIR= $(PWD)
GCC:=gcc
RM:=rm

.PHONY : clean

all: clean modules

obj-m:= S3fserial.o
S3fserial-objs := s3fserial.o hashmap.o linkedlist.o util.o

modules:
	$(MAKE) -C $(KERNEL_SRC) M=$(SUBDIR) modules 

clean:
	$(RM) -f *.ko *.o *.mod.c Module.symvers