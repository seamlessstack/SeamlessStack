CFLAGS += -Wall
CFLAGS += -g
#CFLAGS += -O3
CFLAGS += -I ../common/include -I include -I ../db/mongo/include
CFLAGS += -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -DFUSE_USE_VERSION=26
CFLAGS += -DMONGO_HAVE_STDINT
LIB = $(shell pkg-config --libs fuse) -lpthread -L../db/mongo -lmongo_db
LIB += -L/usr/local/lib -lmongoc -lbson

SFS_OBJS = src/sfs.o

all:: sfs

sfs: $(SFS_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(SFS_OBJS) $(LIB)

clean:
	rm -f sfs *.o
