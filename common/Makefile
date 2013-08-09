CFLAGS += -Wall
CFLAGS += -g
#CFLAGS += -O3
CFLAGS += -I include -I include/bds_slab
CFLAGS += -D_GNU_SOURCE -D_LINUX_ -DUSE_OS_INFO -D_x86_64_
CFLAGS += -fPIC
LIB = -lpthread -lrt
LDFLAGS = -shared -lc -Wl,-soname
SONAME = libsstack_common.so

COMMON_OBJS = src/sstack_thread_pool.o src/sstack_transport_tcp.o \
	      src/bds_slab/bds_slab.o src/bds_slab/bds_sys.o

all: $(COMMON_OBJS)
	$(CC)  $(LIB) $(LDFLAGS),$(SONAME) -o libsstack_common.so $(COMMON_OBJS)

clean:
	rm -f *.so src/*.o src/bds_slab/*.o
