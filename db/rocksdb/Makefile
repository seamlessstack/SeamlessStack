
TOPDIR = ../..
ROCKSDB_OBJECTS = src/rocks_db.o
ROCKSDB_LIBNAME = librocksdb_db.so
CFLAGS += -I $(TOPDIR)/oss/rocksdb/include
CFLAGS += -I $(TOPDIR)/common/include
CFLAGS += -I include 
CFLAGS += -D_GNU_SOURCE -fPIC
LIBS =  -lpthread
CC = gcc
LDFLAGS = -shared -o
ROCKSDB_LIB = -L $(TOPDIR)/oss_install/lib -lrocksdb

all:: $(ROCKSDB_LIBNAME)

$(ROCKSDB_LIBNAME) : $(ROCKSDB_OBJECTS)
	$(CC) $(LDFLAGS) $(ROCKSDB_LIBNAME) $(ROCKSDB_OBJECTS) $(ROCKSDB_LIB) \
						  $(LIBS)

%.o:%.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -f src/*.o $(ROCKSDB_LIBNAME)
