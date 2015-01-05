
TOPDIR = ..
CFLAGS += -Wall
CFLAGS += -g
#CFLAGS += -O3
CFLAGS += -I ./include -I $(TOPDIR)/common/include
CFLAGS += -I $(TOPDIR)/common/include/bds_slab
CFLAGS += -I $(TOPDIR)/policy/include
CFLAGS += -I ../oss/jerasure2.0/include
CFLAGS += -I ../oss/gf-complete/include
CFLAGS += -I $(TOPDIR)/db/mongo/include
CFLAGS += -I $(TOPDIR)/lib/serdes/include
CFLAGS += -I $(TOPDIR)/oss/mongo-c-driver/src/mongoc
CFLAGS += -I $(TOPDIR)/oss/mongo-c-driver/src/libbson/src/bson
CFLAGS += -D_GNU_SOURCE -g
CFLAGS += -Wall
CFLAGS += -DMONGO_HAVE_STDINT
OSS_DIR = $(TOPDIR)/oss/
OSS_LIBDIR = $(TOPDIR)/oss_install/lib
LIB = -lpthread
LIB += -L$(TOPDIR)/common -lsstack_common
LIB += -L$(OSS_LIBDIR) -lJerasure -lmongoc -lbson
LIB += -L$(TOPDIR)/db/mongo -lmongo_db
LIB += -L$(TOPDIR)/lib/serdes -lserdes
LIB += -L$(TOPDIR)/policy -lpolicy
LIB += -L$(TOPDIR)/lib/validate -lvalidate
LIB += -ldl


SFSD_OBJS = src/main.o src/sfsd.o src/sfsd_signals.o src/sfsd_threads.o \
	    src/sfsd_ops.o src/sfsd_transport.o src/sfsd_erasure.o \
	    src/sfsd_chunk.o

all:: sfsd

sfsd: $(SFSD_OBJS) 
	$(CC) $(LDFLAGS) -o $@ $(SFSD_OBJS) $(LIB)

clean:
	rm -f sfsd src/*.o
