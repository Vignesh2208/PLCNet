SRC   = RngStream.cc rng.cc 
HDR  = $(SRC:.cc=.h)
OBJ  = $(SRC:.cc=.o)
CC	= g++
DEBUG	= -g
#CFLAGS	= -Wall -c $(DEBUG) -O0 
CFLAGS	= -Wall -c $(DEBUG) 
#CFLAGS	= -Wall -c

rng.a	: $(OBJ)
	rm -f $@
	ar cq $@ $(OBJ)

%.o : %.cc $(HDR)
	$(CC) $(CFLAGS) -c $<

