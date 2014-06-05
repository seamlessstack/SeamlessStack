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
MONGO_OBJECTS = test_mongo.o
MONGO_TEST = test_mongo
CFLAGS += -I ../include -I $(TOPDIR)/oss/mongo-c-driver/src 
CFLAGS += -I $(TOPDIR)/common/include -DMONGO_HAVE_STDINT
CFLAGS += -D_GNU_SOURCE
CC = gcc
LDFLAGS = -o
MONGOC_LIB = -L $(TOPDIR)/oss_install/lib -lmongoc
BSON_LIB = -L $(TOPDIR)/oss_install/lib -lbson
MONGO_DB_LIB = -L .. -lmongo_db

all:: $(MONGO_TEST)

$(MONGO_TEST) : $(MONGO_OBJECTS)
	$(CC) $(LDFLAGS) $(MONGO_TEST) $(MONGO_OBJECTS) $(MONGOC_LIB) \
								   $(BSON_LIB) $(MONGO_DB_LIB) -lpthread

%.o:%.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -f *.o $(MONGO_TEST)
