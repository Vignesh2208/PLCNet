SRC   = interface.cc entity.cc message.cc inchannel.cc interface.cc outchannel.cc process.cc timeline.cc event.cc
THDR = ../time/pq.h ../time/eventlist.h	../time/stl-eventlist.h

HDR  = $(SRC:.cc=.h) $(THDR) ../s3f.h

OBJ  = $(SRC:.cc=.o) 
CC	= g++
#DEFINE  = -DSAFE_EVT_PTR -DSAFE_MSG_PTR
DEBUG	= -g
#CFLAGS	= -Wall -c $(DEFINE) -I../ -O3
CFLAGS	= -Wall -c $(DEBUG) $(DEFINE) -I../ -O3
#CFLAGS	= -Wall -c $(DEBUG) $(DEFINE) -I../ -O0
LFLAGS	= -Wall $(DEBUG) 

all	: s3f.a

s3f.a	: $(OBJ)
	rm -f $@
	ar cq $@ $(OBJ)

%.o :	%.cc $(HDR)
	$(CC) $(CFLAGS) -c $<

clean:
	rm *.o *.a
