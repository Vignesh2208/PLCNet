SRC    		:= tk_lxc_manager.cc TimeKeeper_functions.cc lxc_proxy.cc utility_functions.cc 
CC	   		:= g++
DEBUG  		:= -g -Wno-write-strings

HDR 		:= $(SRC:.cc=.h) 

OBJ  		:= $(SRC:.cc=.o) 

MY_INCLUDES	:= -I../ 
MY_INCLUDES += -I../s3fnet/src
MY_INCLUDES += -I../dml


CFLAGS  := -Wall $(DEBUG) 

all	: lxcmanagermodule.a

lxcmanagermodule.a	: $(OBJ)
	rm -f $@
	ar cq $@ $(OBJ)

%.o :	%.cc $(HDR)
	$(CC) $(CFLAGS) $(MY_INCLUDES) -c $< 

print-%  : ; @echo $* = $($*)

	
clean:
	rm -f *.o *.a


#all:
#	$(CC) $(CFLAGS) $(SRC) $(MY_INCLUDES)
