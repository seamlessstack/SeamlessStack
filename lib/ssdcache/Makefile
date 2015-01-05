
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
CFLAGS += -D_GNU_SOURCE -fPIC
SSDCACHE_LIB_NAME = libssdcache.so
SSDCACHE_OBJECTS = src/sstack_ssdcache.o
LDFLAGS = -shared -o
OSS_DIR = $(TOPDIR)/oss_install/lib
LIBS += -lpthread
LIBS += -L$(TOPDIR)/oss_install/lib -lrbtree
CC = gcc

all:: $(SSDCACHE_LIB_NAME)

$(SSDCACHE_LIB_NAME) : $(SSDCACHE_OBJECTS)
	$(CC) $(LDFLAGS) $(SSDCACHE_LIB_NAME) $(SSDCACHE_OBJECTS) $(LIBS)

%.o:$.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	$(RM) -f src/*.o $(SSDCACHE_LIB_NAME)

