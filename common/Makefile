CFLAGS += -Wall
CFLAGS += -g
#CFLAGS += -O3
CFLAGS += -I include 
CFLAGS += -D_GNU_SOURCE
CFLAGS += -fPIC
LIB = -lpthread -lrt
LDFLAGS = -shared -lc -Wl,-soname
SONAME = libsstack_common.so

COMMON_OBJS = src/sstack_thread_pool.o src/sstack_transport_tcp.o

all: $(COMMON_OBJS)
	$(CC)  $(LIB) $(LDFLAGS),$(SONAME) -o libsstack_common.so $(COMMON_OBJS)

clean:
	rm -f *.so src/*.o
