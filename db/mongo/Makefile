TOPDIR = ../..
MONGO_OBJECTS = src/mongo_db.o
MONGO_DB_LIBNAME = libmongo_db.so
CFLAGS += -I include -I $(TOPDIR)/common/include -DMONGO_HAVE_STDINT
CFLAGS += -D_GNU_SOURCE -fPIC
CC = gcc
LDFLAGS = -shared -o

all:: $(MONGO_DB_LIBNAME)

$(MONGO_DB_LIBNAME) : $(MONGO_OBJECTS)
	$(CC) $(LDFLAGS) $(MONGO_DB_LIBNAME) $(MONGO_OBJECTS)

%.o:%.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	rm -f src/*.o $(MONGO_DB_LIBNAME)
