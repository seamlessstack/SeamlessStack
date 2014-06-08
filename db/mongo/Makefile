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
MONGO_OBJECTS = src/mongo_db.o
MONGO_DB_LIBNAME = libmongo_db.so
CFLAGS += -I include -I $(TOPDIR)/oss/mongo-c-driver/src/mongoc
CFLAGS += -I $(TOPDIR)/oss/mongo-c-driver/src/libbson/src/bson
CFLAGS += -I $(TOPDIR)/common/include -DMONGO_HAVE_STDINT
CFLAGS += -D_GNU_SOURCE -fPIC -g
CC = gcc
LDFLAGS = -shared -o
MONGOC_LIB = -L $(TOPDIR)/oss_install/lib -lmongoc
BSON_LIB = -L $(TOPDIR)/oss_install/lib -lbson

all:: $(MONGO_DB_LIBNAME)

$(MONGO_DB_LIBNAME) : $(MONGO_OBJECTS)
	$(CC) $(LDFLAGS) $(MONGO_DB_LIBNAME) $(MONGO_OBJECTS) $(MONGOC_LIB) \
								   $(BSON_LIB)

%.o:%.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -f src/*.o $(MONGO_DB_LIBNAME)
