TOPDIR = ../../..
CFALGS += -Wall
CFLAGS += -g
#CFLAGS += -O3
CFLAGS += -I ../include -I$(TOPDIR)/oss_install/include 
CFLAGS += -I $(TOPDIR)/common/include
CFLAGS += -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE
TARGET = test
OBJS = test.o
LDFLAGS = -o
CC = gcc
RM = rm
LIBS = -lpthread
LIBS += -L.. -lmcache
LIBS += -L$(TOPDIR)/oss_install/lib -lmemcached

all:: $(TARGET)

$(TARGET) : $(OBJS)
	$(CC) $(LDFLAGS) $(TARGET) $(OBJS) $(LIBS)

%.o:$.c
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	$(RM) -f *.o $(TARGET)

