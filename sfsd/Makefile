CFLAGS += -Wall
CFLAGS += -g
#CFLAGS += -O3
CFLAGS += -I ./include -I ../common/include -I ../common/include/bds_slab
CFLAGS += -I ../oss/jerasure_lib
CFLAGS += -D_GNU_SOURCE -g
CFLAGS += -Wall
OSS_DIR = $(PWD)/../oss_libs/lib
LIB = -lpthread -lsstack_common -L$(OSS_DIR) -ljerasure
LDFLAGS = -L ../common/ -L ../oss_libs/

SFSD_OBJS = src/main.o src/sfsd.o src/sfsd_signals.o src/sfsd_threads.o \
	    src/sfsd_ops.o src/sfsd_transport.o src/sfsd_erasure.o \

all:: sfsd

sfsd: $(SFSD_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(SFSD_OBJS) $(LIB)

clean:
	rm -f sfsd src/*.o
