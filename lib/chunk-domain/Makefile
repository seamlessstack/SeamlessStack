TOPDIR = ../..
CFALGS += -Wall
CFLAGS += -g
#CFLAGS += -O3
CFLAGS += -D_GNU_SOURCE -fPIC
CFLAGS += -I$(TOPDIR)/common/include
CHUNKAPI_LIB_NAME = libchunkapi.so
CHUNKAPI_OBJECTS = chunk_api.o
LDFLAGS = -shared -o
CC = c99
RM = rm

all:: $(CHUNKAPI_LIB_NAME)

$(CHUNKAPI_LIB_NAME) : $(CHUNKAPI_OBJECTS)
	$(CC) $(LDFLAGS) $(CHUNKAPI_LIB_NAME) $(CHUNKAPI_OBJECTS) $(LIBS)

%.o:$.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	$(RM) -f $(CHUNKAPI_OBJECTS) $(CHUNKAPI_LIB_NAME)
