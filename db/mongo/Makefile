
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
