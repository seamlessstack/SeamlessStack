#  
#  SEAMLESSSTACK CONFIDENTIAL
#  __________________________
#  
#   [2012] - [2014]  SeamlessStack Inc
#   All Rights Reserved.
#  
#  NOTICE:  All information contained herein is, and remains
#  the property of SeamlessStack Incorporated and its suppliers,
#  if any.  The intellectual and technical concepts contained
#  herein are proprietary to SeamlessStack Incorporated
#  and its suppliers and may be covered by U.S. and Foreign Patents,
#  patents in process, and are protected by trade secret or copyright law.
#  Dissemination of this information or reproduction of this material
#  is strictly forbidden unless prior written permission is obtained
#  from SeamlessStack Incorporated.
#

TOPDIR = ../../..
CFALGS += -Wall
CFLAGS += -g
#CFLAGS += -O3
CFLAGS += -I ../include -I $(TOPDIR)/oss_install/include
CFLAGS += -I $(TOPDIR)/common/include
CFLAGS += -I $(TOPDIR)/oss/rbtree
CFLAGS += -I $(TOPDIR)/policy/include
CFLAGS += -I $(TOPDIR)/lib/memcache/include
CFLAGS += -I $(TOPDIR)/common/include/bds_slab
CFLAGS += -I $(TOPDIR)/lib/ssdcache/include
CFLAGS += -D_GNU_SOURCE -fPIC
OSS_DIR = $(TOPDIR)/oss_install/lib
LIBS = -L$(OSS_DIR) -lmemcached
LIBS += -lpthread
LIBS += -L$(TOPDIR)/oss_install/lib -lrbtree
LIBS += -L$(TOPDIR)/lib/memcache -lmcache
LIBS += -L$(TOPDIR)/lib/cache -lcache
LIBS += -L$(TOPDIR)/lib/ssdcache -lssdcache
LIBS += -ldl
TARGET = test
OBJS = test_cache.o
RM = rm
CC = gcc
LDFLAGS = -o

all:: $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) $(LDFLAGS) $(TARGET) $(OBJS) $(LIBS)

%.o:$.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	$(RM) -f *.o $(TARGET)

