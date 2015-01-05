
TOPDIR = ../../../..
CFALGS += -Wall
CFLAGS += -g
#CFLAGS += -O3
CFLAGS += -D_GNU_SOURCE -fPIC
CFLAGS += -I$(TOPDIR)/policy/include
CFLAGS += -I$(TOPDIR)/common/include
COMPRESSION_LIB_NAME = libcompressplugin.so
COMPRESSION_OBJECTS = sstack_compression_plugin.o
LDFLAGS = -shared -o
LIBS = -llzma -lpthread
CC = c99
RM = rm

all:: $(COMPRESSION_LIB_NAME)

$(COMPRESSION_LIB_NAME) : $(COMPRESSION_OBJECTS)
	$(CC) $(LDFLAGS) $(COMPRESSION_LIB_NAME) $(COMPRESSION_OBJECTS) $(LIBS)

%.o:$.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	$(RM) -f $(COMPRESSION_OBJECTS) $(COMPRESSION_LIB_NAME)

