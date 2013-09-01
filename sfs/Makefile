#  
#  SEAMLESSSTACK CONFIDENTIAL
#  __________________________
#  
#   [2012] - [2013]  SeamlessStack Inc
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
TOPDIR=..
CFLAGS += -Wall
CFLAGS += -g
#CFLAGS += -O3
CFLAGS += -I $(TOPDIR)/common/include -I include -I $(TOPDIR)/db/mongo/include
CFLAGS += -I $(TOPDIR)/common/include/bds_slab
CFLAGS += -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -DFUSE_USE_VERSION=26
CFLAGS += -DMONGO_HAVE_STDINT
OSS_LIBSDIR = $(TOPDIR)/oss_install/lib
LIB = $(shell pkg-config --libs fuse) -lpthread -L$(TOPDIR)/db/mongo -lmongo_db
LIB += -L$(OSS_LIBSDIR) -lmongoc -lbson

SFS_OBJS = src/sfs.o

all:: sfs

sfs: $(SFS_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(SFS_OBJS) $(LIB)

clean:
	rm -f sfs *.o
