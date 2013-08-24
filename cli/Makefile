CFLAGS += -Wall
CFLAGS += -g
#CFLAGS += -O3
CFLAGS += -I ../common/include -I include -I ../sfs/include
CFLAGS += -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE
LIBS = -lpthread
RM = rm
CC = gcc

LOG_DECODE_OBJS = src/sstack_log_decode.o
SFSCLI_OBJS = src/sfscli.o

all:: sstack_log_decode sfscli

sstack_log_decode: $(LOG_DECODE_OBJS)
	$(CC) -o $@ $(LOG_DECODE_OBJS) $(LIBS)

sfscli: $(SFSCLI_OBJS)
	$(CC) -o $@ $(SFSCLI_OBJS) $(LIBS)

clean:
	$(RM) -f sstack_log_decode sfscli src/*.o

