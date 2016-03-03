SRC   = barrier.cc barrier_mutex.cc fast_barrier.cc fast_tree_barrier.cc
HDR  = $(SRC:.cc=.h)
OBJ  = $(SRC:.cc=.o)
OPENVZEMU_INCLUDES=../openvzemu_include
CC	= g++
DEBUG	= -g
#CFLAGS	= -Wall -c -O3 -I../ -I$(OPENVZEMU_INCLUDES)
CFLAGS	= -Wall -c $(DEBUG) -O3 -I../ -I$(OPENVZEMU_INCLUDES)
#CFLAGS	= -Wall -c $(DEBUG) -O0 -I../ -I$(OPENVZEMU_INCLUDES)

aux.a	: $(OBJ)
	rm -f $@
	ar cq aux.a $(OBJ)

%.o : %.cc $(HDR)
	$(CC) $(CFLAGS) -c $<

clean:
	rm *.o *.a

