
TOPDIR = ../..
CFALGS += -Wall
CFLAGS += -g
#CFLAGS += -O3
CFLAGS += -D_GNU_SOURCE -fPIC
CFLAGS += -I include
LZ4_LIBNAME = liblz4.so
LZ4_OBJECTS = src/lz4.o src/lz4hc.o src/xxhash.o
LDFLAGS = -shared -o
CC = c99
RM = rm

all:: $(LZ4_LIBNAME)

$(LZ4_LIBNAME) : $(LZ4_OBJECTS)
	$(CC) $(LDFLAGS) $(LZ4_LIBNAME) $(LZ4_OBJECTS) $(LIBS)

%.o:$.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	$(RM) -f $(LZ4_OBJECTS) $(LZ4_LIBNAME)

