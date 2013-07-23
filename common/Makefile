CFLAGS += -Wall
CFLAGS += -g
#CFLAGS += -O3
CFLAGS += -I include 
CFLAGS += -D_GNU_SOURCE
CFLAGS += -fPIC
LIB = -lpthread 
LDFLAGS = -shared -Wl,-soname 

COMMON_OBJS = src/sstack_threads.o

all:: sstack_threads 

sstack_threads: $(COMMON_OBJS)
	$(CC) $(COMMON_OBJS) $(LIB) $(LDFLAGS) -o libsstack_common.so 

clean:
	rm -f *.so src/*.o
