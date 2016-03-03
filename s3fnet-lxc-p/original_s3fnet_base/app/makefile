SRC   = phold_node.cc 
SRC2   = phold_node2.cc 
HDR  = $(SRC:.cc=.h)
HDR2  = $(SRC2:.cc=.h)
OBJ  = $(SRC:.cc=.o)
OBJ2  = $(SRC2:.cc=.o)
#DEFINE = -DSAFE_EVT_PTR -DSAFE_MSG_PTR
CC	= g++
DEBUG	= -g
#CFLAGS	= -Wall -c $(DEFINE) -I../ -O3
CFLAGS	= -Wall -c $(DEBUG) $(DEFINE) -I../ -O3
#CFLAGS	= -Wall -c $(DEBUG) $(DEFINE) -I../ -O0
LFLAGS	= -Wall $(DEBUG) -L../api -lpthread
S3FLIB	= ../api/s3f.a
RNDLIB  = ../rng/rng.a
AUXLIB  = ../aux/aux.a

phold_node	: phold_node.o $(S3FLIB) $(RNDLIB) $(AUXLIB)
	$(CC) -o phold_node phold_node.o $(S3FLIB) $(RNDLIB) $(AUXLIB) $(LFLAGS) 

phold_node2	: phold_node2.o $(S3FLIB) $(RNDLIB) $(AUXLIB)
	$(CC) -o phold_node2 phold_node2.o $(S3FLIB) $(RNDLIB) $(AUXLIB) $(LFLAGS) 

phold_node.o	: $(SRC) $(HDR) 
	$(CC) $(CFLAGS) -c $<

phold_node2.o	: $(SRC2) $(HDR2) 
	$(CC) $(CFLAGS) -c $<

