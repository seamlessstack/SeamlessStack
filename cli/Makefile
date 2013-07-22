CFLAGS += -Wall
CFLAGS += -g
#CFLAGS += -O3
CFLAGS += -I ../common/include -I include
CFLAGS += -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE
LIBS = -lpthread

LOG_DECODE_OBJS = src/sstack_log_decode.o

all:: sstack_log_decode

sstack_log_decode: $(LOG_DECODE_OBJS)
	$(CC) -o $@ $(LOG_DECODE_OBJS) $(LIBS)

clean:
	rm -f sstack_log_decode src/*.o

