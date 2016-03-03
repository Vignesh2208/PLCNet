EXTRA_CFLAGS += 
KERNEL_SRC:= /lib/modules/$(shell uname -r)/build
SUBDIR= $(PWD)
GCC:=gcc
RM:=rm

.PHONY : clean

all: clean modules

obj-m:= SocketHook.o
SocketHook-objs := socket_module.o general_commands.o hooked_functions.o

modules:
	$(MAKE) -C $(KERNEL_SRC) M=$(SUBDIR) modules 

clean:
	$(RM) -f *.ko *.o *.mod.c Module.symvers
