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

TOPDIR = ../..
ROCKSDB_OBJECTS = src/rocksdb_db.o
ROCKSDB_LIBNAME = librocksdb_db.so
CFLAGS += -I include 
CFLAGS += -I $(TOPDIR)/common/include
CFLAGS += -D_GNU_SOURCE -fPIC
CC = gcc
LDFLAGS = -shared -o
ROCKSDB_LIB = -L $(TOPDIR)/oss_install/lib -lmongoc

all:: $(ROCKSDB_LIBNAME)

$(ROCKSDB_LIBNAME) : $(ROCKSDB_OBJECTS)
	$(CC) $(LDFLAGS) $(ROCKSDB_LIBNAME) $(ROCKSDB_OBJECTS) $(ROCKSDB_LIB)

%.o:%.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -f src/*.o $(ROCKSDB_LIBNAME)
