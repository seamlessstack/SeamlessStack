
TOPDIR = ../..
CFALGS += -Wall
CFLAGS += -g
#CFLAGS += -O3
CFLAGS += -I ./include -I $(TOPDIR)/oss_install/include
CFLAGS += -I $(TOPDIR)/common/include
CFLAGS += -D_GNU_SOURCE -fPIC
CACHING_LIB_NAME = libmcache.so
CACHING_OBJECTS = src/sstack_cache_api.o
LDFLAGS = -shared -o
OSS_DIR = $(TOPDIR)/oss_install/lib
LIBS = -L$(OSS_DIR) -lmemcached
CC = gcc

all:: $(CACHING_LIB_NAME)

$(CACHING_LIB_NAME) : $(CACHING_OBJECTS)
	$(CC) $(LDFLAGS) $(CACHING_LIB_NAME) $(CACHING_OBJECTS) $(LIBS)

%.o:$.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	$(RM) -f src/*.o $(CACHING_LIB_NAME)

