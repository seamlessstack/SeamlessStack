TOPDIR = ..
CFLAGS += -Wall
CFLAGS += -g
#CFLAGS += -O3
CFLAGS += -I ./include -I $(TOPDIR)/common/include
CFLAGS += -I $(TOPDIR)/common/include/bds_slab
CFLAGS += -I ../oss/jerasure_lib
CFLAGS += -D_GNU_SOURCE -g
CFLAGS += -Wall
OSS_DIR = $(TOPDIR)/oss_install/
OSS_LIBDIR = $(OSS_DIR)/lib
LIB = -lpthread -lsstack_common -ljerasure
LDFLAGS = -L $(TOPDIR)/common/ -L $(OSS_LIBDIR)

SFSD_OBJS = src/main.o src/sfsd.o src/sfsd_signals.o src/sfsd_threads.o \
	    src/sfsd_ops.o src/sfsd_transport.o src/sfsd_erasure.o \

all:: sfsd

sfsd: $(SFSD_OBJS) 
	$(CC) $(LDFLAGS) -o $@ $(SFSD_OBJS) $(LIB)

clean:
	rm -f sfsd src/*.o
