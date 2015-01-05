
TOPDIR = ../..
CFALGS += -Wall
CFLAGS += -g
#CFLAGS += -O3
CFLAGS += -I ./include -I $(TOPDIR)/oss_install/include
CFLAGS += -I $(TOPDIR)/common/include
CFLAGS += -I $(TOPDIR)/oss/rbtree
CFLAGS += -I $(TOPDIR)/policy/include
CFLAGS += -I $(TOPDIR)/lib/memcache/include
CFLAGS += -I $(TOPDIR)/common/include/bds_slab
CFLAGS += -I $(TOPDIR)/lib/ssdcache/include
CFLAGS += -D_GNU_SOURCE -fPIC
GENCACHE_LIB_NAME = libcache.so
GENCACHE_OBJECTS = src/sstack_cache.o src/sstack_lrucache.o
LDFLAGS = -shared -o
OSS_DIR = $(TOPDIR)/oss_install/lib
LIBS = -L$(OSS_DIR) -lmemcached
LIBS += -lpthread
LIBS += -L$(TOPDIR)/oss_install/lib -lrbtree
LIBS += -L$(TOPDIR)/lib/memcache -lmcache
LIBS += -ldl
CC = gcc

all:: $(GENCACHE_LIB_NAME)

$(GENCACHE_LIB_NAME) : $(GENCACHE_OBJECTS)
	$(CC) $(LDFLAGS) $(GENCACHE_LIB_NAME) $(GENCACHE_OBJECTS) $(LIBS)

%.o:$.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	$(RM) -f src/*.o $(GENCACHE_LIB_NAME)

