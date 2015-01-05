
TOPDIR = ../..
CFALGS += -Wall
CFLAGS += -g
#CFLAGS += -O3
CFLAGS += -fPIC -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE
CFLAGS += -I$(TOPDIR)/common/include -I$(TOPDIR)/protobuf-template/proto
CFLAGS += -Iinclude -I$(TOPDIR)/common/include/bds_slab
CFLAGS += -I$(TOPDIR)/policy/include
SERDES_LIB_NAME = libserdes.so
SERDES_OBJECTS = src/sstack_serdes.o
SERDES_OBJECTS += $(TOPDIR)/protobuf-template/proto/jobs.pb-c.o
LDFLAGS = -shared -o
LIBS = -lprotobuf-c
CC = gcc
RM = rm

all:: $(SERDES_LIB_NAME)

$(SERDES_LIB_NAME) : $(SERDES_OBJECTS)
	$(CC) $(LDFLAGS) $(SERDES_LIB_NAME) $(SERDES_OBJECTS) $(LIBS)

%.o:$.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	$(RM) -f $(SERDES_OBJECTS) $(SERDES_LIB_NAME)

