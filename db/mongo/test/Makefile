
TOPDIR = ../../..
MONGO_OBJECTS = test_mongo.o
MONGO_TEST = test_mongo
CFLAGS += -I ../include -I $(TOPDIR)/oss/mongo-c-driver/src/mongoc
CFLAGS += -I ../include -I $(TOPDIR)/oss/mongo-c-driver/src/libbson/src/bson
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
